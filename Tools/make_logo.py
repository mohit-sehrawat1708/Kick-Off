"""Draws the Pitch Kings logo (crest + word mark) to Tools/art/PK_Logo.png and PK_Crest.png.

Run: python Tools/make_logo.py   (needs Pillow). Imported into /Game/PitchKings/UI by
Tools/editor_scripts/import_ui.py.
Other names: python Tools/make_logo.py --words KICK OFF --out Website/img/logo.png   (word mark only, the crest stays)
"""
import math
import os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "art")
os.makedirs(OUT, exist_ok=True)
FONTS = "C:/Windows/Fonts/"

GOLD = (255, 200, 40)
GOLD_DARK = (190, 120, 10)
NAVY = (10, 22, 52)
BLUE = (30, 110, 235)
WHITE = (245, 245, 245)


def gradient(size, top, bottom):
    w, h = size
    g = Image.new("RGBA", size)
    d = ImageDraw.Draw(g)
    for y in range(h):
        t = y / max(1, h - 1)
        d.line([(0, y), (w, y)], fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,))
    return g


def crest(size=512):
    """Shield with a crown over a football."""
    S = size * 4  # supersample
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx = S / 2

    def shield(scale, fill):
        w, top, bot = S * 0.40 * scale, S * 0.10 + (1 - scale) * S * 0.4, S * 0.94 - (1 - scale) * S * 0.4
        pts = [(cx - w, top), (cx + w, top), (cx + w, top + (bot - top) * 0.55)]
        for k in range(1, 30):  # curved bottom to the point
            a = k / 30
            pts.append((cx + w * (1 - a) ** 1.2, top + (bot - top) * (0.55 + 0.45 * a)))
        pts.append((cx, bot))
        for k in range(29, 0, -1):
            a = k / 30
            pts.append((cx - w * (1 - a) ** 1.2, top + (bot - top) * (0.55 + 0.45 * a)))
        pts.append((cx - w, top + (bot - top) * 0.55))
        d.polygon(pts, fill=fill)

    shield(1.0, GOLD + (255,))
    shield(0.93, NAVY + (255,))
    # blue diagonal stripes inside
    stripes = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    sd = ImageDraw.Draw(stripes)
    for k in range(-6, 8):
        x0 = cx + k * S * 0.11
        sd.polygon([(x0, 0), (x0 + S * 0.05, 0), (x0 + S * 0.05 - S, S), (x0 - S, S)], fill=BLUE + (70,))
    mask = Image.new("L", (S, S), 0)
    md = ImageDraw.Draw(mask)
    md.rectangle([0, 0, S, S], fill=0)
    # reuse the inner shield as a mask
    inner = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    dd = ImageDraw.Draw(inner)
    d_saved = d
    d = dd
    shield(0.93, (255, 255, 255, 255))
    d = d_saved
    img.alpha_composite(Image.composite(stripes, Image.new("RGBA", (S, S)), inner.split()[3]))

    # football
    r = S * 0.17
    by = S * 0.58
    d.ellipse([cx - r, by - r, cx + r, by + r], fill=WHITE + (255,), outline=(20, 20, 20, 255), width=int(S * 0.012))
    pent = [(cx + r * 0.42 * math.cos(math.radians(-90 + 72 * k)), by + r * 0.42 * math.sin(math.radians(-90 + 72 * k))) for k in range(5)]
    d.polygon(pent, fill=(20, 20, 20, 255))
    for (px, py) in pent:
        ang = math.atan2(py - by, px - cx)
        d.line([(px, py), (cx + r * 0.95 * math.cos(ang), by + r * 0.95 * math.sin(ang))], fill=(20, 20, 20, 255), width=int(S * 0.012))

    # crown
    base_y = S * 0.36
    cw = S * 0.26
    crown = [(cx - cw, base_y), (cx - cw, base_y - S * 0.09), (cx - cw * 0.55, base_y - S * 0.035), (cx - cw * 0.3, base_y - S * 0.15),
             (cx, base_y - S * 0.05), (cx + cw * 0.3, base_y - S * 0.15), (cx + cw * 0.55, base_y - S * 0.035), (cx + cw, base_y - S * 0.09), (cx + cw, base_y)]
    d.polygon(crown, fill=GOLD + (255,), outline=GOLD_DARK + (255,))
    d.rectangle([cx - cw, base_y - S * 0.004, cx + cw, base_y + S * 0.035], fill=GOLD_DARK + (255,))
    for px, py in [(cx - cw, base_y - S * 0.09), (cx - cw * 0.3, base_y - S * 0.15), (cx + cw * 0.3, base_y - S * 0.15), (cx + cw, base_y - S * 0.09)]:
        rr = S * 0.022
        d.ellipse([px - rr, py - rr, px + rr, py + rr], fill=WHITE + (255,))
    return img.resize((size, size), Image.LANCZOS)


def text_layer(size, text, font, fill_top, fill_bottom, stroke, stroke_w, pos):
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    m = Image.new("L", size, 0)
    ImageDraw.Draw(m).text(pos, text, font=font, fill=255)
    so = Image.new("L", size, 0)
    ImageDraw.Draw(so).text(pos, text, font=font, fill=255, stroke_width=stroke_w, stroke_fill=255)
    layer.paste(Image.new("RGBA", size, stroke + (255,)), (0, 0), so)
    layer.paste(gradient(size, fill_top, fill_bottom), (0, 0), m)
    return layer


def logo(top="PITCH", bottom="KINGS"):
    W, H = 1240, 560
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    c = crest(500)
    # soft glow behind crest
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([110, 90, 450, 470], fill=(255, 190, 40, 90))  # well inside the image: the blur never reaches the edge
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(30)))
    img.alpha_composite(c, (30, 30))

    big = ImageFont.truetype(FONTS + "impact.ttf", 215)
    small = ImageFont.truetype(FONTS + "bahnschrift.ttf", 62)
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).text((580, 40), top, font=big, fill=(0, 0, 0, 170))
    ImageDraw.Draw(shadow).text((580, 250), bottom, font=big, fill=(0, 0, 0, 170))
    img.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(8)))
    img.alpha_composite(text_layer((W, H), top, big, (255, 255, 255), (200, 215, 240), NAVY, 10, (565, 22)))
    img.alpha_composite(text_layer((W, H), bottom, big, (255, 222, 90), (230, 150, 20), NAVY, 10, (565, 232)))
    tag = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(tag).text((585, 548), "F O O T B A L L", font=small, fill=(220, 230, 255, 230), anchor="ls")
    img.alpha_composite(tag)
    return img


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--words", nargs=2, metavar=("TOP", "BOTTOM"), help="the two lines of the word mark")
    ap.add_argument("--out", help="write only the logo, to this file")
    a = ap.parse_args()
    if a.words or a.out:
        top, bottom = a.words or ("PITCH", "KINGS")
        logo(top, bottom).save(a.out or os.path.join(OUT, "PK_Logo.png"))
        print("wrote", a.out)
    else:
        logo().save(os.path.join(OUT, "PK_Logo.png"))
        crest(512).save(os.path.join(OUT, "PK_Crest.png"))
        print("wrote", OUT)
