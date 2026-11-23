#!/usr/bin/env python3
"""Значок NaomiPhone Mirror: окно Mac (светофор в заголовке), в нём — экран телефона (часы, строки главного), поверх —
курсор Mac: экран телефона в окне, и мышь им управляет. Рисуется здесь, без картинок: icon.png (1024) и icon.icns (их
кладёт в приложение build.sh). Шрифты — Google Sans Flex и Google Sans из downloads/fonts.

    python3 mirror/make_icon.py      (Python с Pillow)
"""
import math
import os
import subprocess
import tempfile

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONTS = os.path.join(HERE, "..", "downloads", "fonts")   # шрифты интерфейса: tools/naomi fonts
FLEX = os.path.join(FONTS, "GoogleSansFlex[ROND,opsz,slnt,wdth,wght].ttf")
SANS = os.path.join(FONTS, "GoogleSans[GRAD,opsz,wght].ttf")
S = 2048  # drawn at twice the size, then made smaller: smooth edges
TEXT = (236, 233, 227)     # the phone's Theme.text
MUTED = (169, 164, 154)    # Theme.muted
AMBER = (216, 178, 90)     # Theme.amber


def squircle(size, box, n=5.0):
    """The Mac icon's tile: a superellipse (|x|^n + |y|^n = 1) — the corners round off smoothly, not as a circle's quarter"""
    x0, y0, x1, y1 = box
    cx, cy, rx, ry = (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) / 2, (y1 - y0) / 2
    pts = []
    for i in range(720):
        t = 2 * math.pi * i / 720
        c, s = math.cos(t), math.sin(t)
        pts.append((cx + rx * math.copysign(abs(c) ** (2 / n), c), cy + ry * math.copysign(abs(s) ** (2 / n), s)))
    m = Image.new("L", (size, size), 0)
    ImageDraw.Draw(m).polygon(pts, fill=255)
    return m


def vgrad(size, top, bottom):
    g = Image.new("RGB", (1, 256))
    for y in range(256):
        k = y / 255
        g.putpixel((0, y), tuple(round(a + (b - a) * k) for a, b in zip(top, bottom)))
    return g.resize((size, size), Image.BILINEAR)


def rounded(size, box, r):
    m = Image.new("L", (size, size), 0)
    ImageDraw.Draw(m).rounded_rectangle(box, radius=r, fill=255)
    return m


def flex(px, wght):
    f = ImageFont.truetype(FLEX, px)
    f.set_variation_by_axes([144, 100, wght, 100, 0])  # opsz, wdth, wght, ROND (rounded, as the phone's clock), slnt
    return f


def sans(px, wght):
    f = ImageFont.truetype(SANS, px)
    f.set_variation_by_axes([18, wght, 0])  # opsz, wght, GRAD
    return f


def spaced(d, x, y, text, font, fill, gap):
    """Caps with letter spacing (the phone's Naomi.Ui Caps), x — the middle"""
    widths = [font.getlength(c) for c in text]
    total = sum(widths) + gap * (len(text) - 1)
    at = x - total / 2
    for c, w in zip(text, widths):
        d.text((at, y), c, font=font, fill=fill)
        at += w + gap


LIGHTS = ((255, 95, 87), (254, 188, 46), (40, 200, 64))   # светофор окна Mac


def draw():
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    # плашка значка Mac (824 из 1024, посередине) с тенью
    tile_box = (S * 100 / 1024, S * 100 / 1024, S * 924 / 1024, S * 924 / 1024)
    tile = squircle(S, tile_box)
    shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    shadow.putalpha(tile.point(lambda v: v * 0.45))
    shadow = ImageChops.offset(shadow, 0, S // 90).filter(ImageFilter.GaussianBlur(S / 60))
    img.alpha_composite(shadow)
    body = vgrad(S, (44, 46, 52), (14, 15, 17)).convert("RGBA")
    body.putalpha(tile)
    img.alpha_composite(body)
    rim = Image.new("RGBA", (S, S), (255, 255, 255, 0))
    inner = squircle(S, tuple(v + d for v, d in zip(tile_box, (4, 6, -4, -2))))
    fade = vgrad(S, (255, 255, 255), (0, 0, 0)).convert("L").point(lambda v: v * 0.35)
    rim.putalpha(ImageChops.multiply(ImageChops.subtract(tile, inner), fade))
    img.alpha_composite(rim)

    # окно: заголовок со светофором, под ним — экран телефона 393 × 851 точки
    cx = S / 2
    H = S * 0.600                  # экран
    W = H * 393 / 851
    bar = W * 0.15                 # заголовок окна
    x0 = cx - W / 2
    y0 = S / 2 - (H + bar) / 2 + S * 0.004
    x1, ys, y1 = x0 + W, y0 + bar, y0 + bar + H
    r = W * 0.085
    ws = Image.new("RGBA", (S, S), (0, 0, 0, 0))   # тень окна на плашке
    ws.putalpha(rounded(S, (x0, y0 + S * 0.014, x1, y1 + S * 0.014), r).point(lambda v: v * 0.6))
    img.alpha_composite(ws.filter(ImageFilter.GaussianBlur(S / 60)))
    frame = vgrad(S, (58, 60, 66), (36, 37, 41)).convert("RGBA")
    frame.putalpha(rounded(S, (x0, y0, x1, y1), r))
    img.alpha_composite(frame)
    screen = Image.new("RGBA", (S, S), (0, 0, 0, 255))   # экран — во всё окно под заголовком, углы окна снизу
    m = rounded(S, (x0, ys, x1, y1), r)
    m.paste(0, (0, 0, S, int(ys)))
    sq = Image.new("L", (S, S), 0)
    ImageDraw.Draw(sq).rectangle((x0, ys, x1, ys + r), fill=255)
    screen.putalpha(ImageChops.lighter(m, sq))
    img.alpha_composite(screen)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((x0, y0, x1, y1), radius=r, outline=(84, 87, 95, 255), width=max(2, int(S * 0.0025)))
    lr = bar * 0.17
    for i, c in enumerate(LIGHTS):
        lx, ly = x0 + bar * 0.48 + i * lr * 3.1, y0 + bar / 2
        d.ellipse((lx - lr, ly - lr, lx + lr, ly + lr), fill=c + (255,))

    k = W / 393   # точка телефона, пиксели здесь
    # часы экрана (Google Sans Flex, ROND, светлые) и дата капсом
    t = "17:58"
    f = flex(int(k * 118), 340)
    l, tp, rr, b = f.getbbox(t)
    tx, ty = cx - (l + rr) / 2, ys + H * 0.17 - tp
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(glow).text((tx, ty), t, font=f, fill=TEXT + (90,))
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(S / 90)))
    d = ImageDraw.Draw(img)
    d.text((tx, ty), t, font=f, fill=TEXT + (255,))
    spaced(d, cx, ty + b + S * 0.022, "THURSDAY · 12", sans(int(k * 15), 500), MUTED + (255,), k * 2.2)
    # строки главного внизу: точка значка, полоска названия
    rows = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    dr = ImageDraw.Draw(rows)
    for i, (wide, lit) in enumerate(((0.42, True), (0.34, False), (0.38, False))):
        ry = y1 - H * 0.235 + i * 68 * k * 0.86
        dr.ellipse((x0 + 26 * k, ry - 9 * k, x0 + 44 * k, ry + 9 * k), fill=(AMBER if lit else MUTED) + (255 if lit else 150,))
        dr.rounded_rectangle((x0 + 62 * k, ry - 6 * k, x0 + 62 * k + W * wide, ry + 6 * k), radius=6 * k,
                             fill=TEXT + (215 if lit else 110,))
    img.alpha_composite(rows)

    # курсор Mac над первой строкой: белая стрелка с тёмной обводкой и тенью
    u = S * 0.00125                # шаг рисунка стрелки: стрелка ≈ 0,14 плашки
    ax, ay = x0 + 62 * k + W * 0.36, y1 - H * 0.235 - 2 * k   # остриё — на конце первой строки
    arrow = [(0, 0), (0, 100), (24, 77), (40, 114), (56, 107), (40, 71), (73, 71)]
    pts = [(ax + px * u, ay + py * u) for px, py in arrow]
    cs = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(cs).polygon([(x + S * 0.004, y + S * 0.008) for x, y in pts], fill=(0, 0, 0, 150))
    img.alpha_composite(cs.filter(ImageFilter.GaussianBlur(S / 160)))
    d = ImageDraw.Draw(img)
    d.polygon(pts, fill=(255, 255, 255, 255), outline=(10, 10, 12, 255), width=max(3, int(u * 6)))
    return img.resize((1024, 1024), Image.LANCZOS)


def main():
    icon = draw()
    png = os.path.join(HERE, "icon.png")
    icon.save(png)
    with tempfile.TemporaryDirectory() as tmp:
        folder = os.path.join(tmp, "icon.iconset")
        os.mkdir(folder)
        for px in (16, 32, 128, 256, 512):
            icon.resize((px, px), Image.LANCZOS).save(os.path.join(folder, f"icon_{px}x{px}.png"))
            icon.resize((px * 2, px * 2), Image.LANCZOS).save(os.path.join(folder, f"icon_{px}x{px}@2x.png"))
        subprocess.run(["iconutil", "-c", "icns", folder, "-o", os.path.join(HERE, "icon.icns")], check=True)
    print(png, os.path.join(HERE, "icon.icns"))


if __name__ == "__main__":
    main()
