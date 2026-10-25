#!/usr/bin/env python3
"""Generates the QoL menu icons for every theme, in each theme's own palette.

Icons are drawn as 1-bit pixel art (ink + shade), then encoded as LVGL indexed images
with the palette taken from the theme's stock settings icon, so they match the art.

Usage: gen_icons.py [--preview out.png]   (run from the repo root)
"""
import math
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(__file__))
import lvimg  # noqa: E402

W, H = 48, 49
INK, SHADE = 1, 2


def canvas():
    im = Image.new('L', (W, H), 0)
    return im, ImageDraw.Draw(im)


def ring(d, cx, cy, r, width=2, fill=INK):
    for t in range(width):
        d.ellipse((cx - r + t, cy - r + t, cx + r - t, cy + r - t), outline=fill)


def stopwatch():
    im, d = canvas()
    cx, cy, r = 24, 28, 17
    ring(d, cx, cy, r)
    d.rectangle((21, 4, 27, 7), fill=INK)          # top button
    d.rectangle((23, 7, 25, 10), fill=INK)
    d.line((36, 12, 39, 9), fill=INK, width=2)     # side button
    for a in range(0, 360, 30):                    # ticks
        x1 = cx + (r - 4) * math.sin(math.radians(a)); y1 = cy - (r - 4) * math.cos(math.radians(a))
        d.point((round(x1), round(y1)), fill=SHADE)
    d.line((cx, cy, cx, cy - 11), fill=INK, width=2)
    d.line((cx, cy, cx + 7, cy + 4), fill=SHADE, width=2)
    d.ellipse((cx - 2, cy - 2, cx + 2, cy + 2), fill=INK)
    return im


def timer():
    im, d = canvas()
    d.rectangle((10, 4, 38, 7), fill=INK)
    d.rectangle((10, 42, 38, 45), fill=INK)
    d.line((13, 8, 13, 13), fill=INK, width=2); d.line((35, 8, 35, 13), fill=INK, width=2)
    d.line((13, 13, 22, 24), fill=INK, width=2); d.line((35, 13, 26, 24), fill=INK, width=2)
    d.line((22, 25, 13, 36), fill=INK, width=2); d.line((26, 25, 35, 36), fill=INK, width=2)
    d.line((13, 36, 13, 41), fill=INK, width=2); d.line((35, 36, 35, 41), fill=INK, width=2)
    d.polygon([(17, 14), (31, 14), (24, 22)], fill=SHADE)   # sand top
    d.line((24, 23, 24, 34), fill=SHADE)                    # falling sand
    d.polygon([(16, 40), (32, 40), (24, 33)], fill=SHADE)   # sand bottom
    return im


def alarm():
    im, d = canvas()
    cx, cy, r = 24, 26, 15
    ring(d, cx, cy, r)
    d.pieslice((3, 3, 19, 19), 180, 270, fill=INK)          # bells
    d.pieslice((29, 3, 45, 19), 270, 360, fill=INK)
    d.line((14, 38, 9, 45), fill=INK, width=2); d.line((34, 38, 39, 45), fill=INK, width=2)
    d.line((cx, cy, cx, cy - 9), fill=INK, width=2)
    d.line((cx, cy, cx + 7, cy), fill=SHADE, width=2)
    d.ellipse((cx - 2, cy - 2, cx + 2, cy + 2), fill=INK)
    d.line((21, 2, 27, 2), fill=SHADE); d.line((24, 2, 24, 9), fill=SHADE)
    return im


def globe():
    im, d = canvas()
    cx, cy, r = 24, 24, 19
    ring(d, cx, cy, r)
    d.ellipse((cx - 9, cy - r, cx + 9, cy + r), outline=SHADE)
    d.line((cx, cy - r, cx, cy + r), fill=SHADE)
    d.line((cx - r, cy, cx + r, cy), fill=INK)
    for dy in (-10, 10):
        w = int(math.sqrt(r * r - dy * dy))
        d.line((cx - w, cy + dy, cx + w, cy + dy), fill=SHADE)
    d.ellipse((34, 33, 46, 45), fill=0, outline=INK)        # little clock badge
    d.line((40, 36, 40, 39), fill=INK); d.line((40, 39, 43, 39), fill=INK)
    return im


def weather():
    im, d = canvas()
    cx, cy = 19, 18
    d.ellipse((cx - 8, cy - 8, cx + 8, cy + 8), fill=SHADE)
    ring(d, cx, cy, 8, 1)
    for a in range(0, 360, 45):
        x1 = cx + 11 * math.cos(math.radians(a)); y1 = cy + 11 * math.sin(math.radians(a))
        x2 = cx + 15 * math.cos(math.radians(a)); y2 = cy + 15 * math.sin(math.radians(a))
        d.line((x1, y1, x2, y2), fill=INK, width=2)
    # cloud
    d.ellipse((14, 24, 30, 40), fill=0, outline=INK, width=2)
    d.ellipse((24, 18, 42, 36), fill=0, outline=INK, width=2)
    d.ellipse((34, 28, 46, 40), fill=0, outline=INK, width=2)
    d.rectangle((20, 33, 40, 40), fill=0)
    d.line((20, 41, 40, 41), fill=INK, width=2)
    d.line((14, 34, 14, 36), fill=INK); d.line((46, 34, 46, 36), fill=INK)
    return im


def sysinfo():
    im, d = canvas()
    d.rectangle((12, 12, 36, 36), outline=INK, width=2)
    d.rectangle((17, 17, 31, 31), fill=SHADE)
    for i in range(4):
        p = 15 + i * 6
        d.line((p, 5, p, 11), fill=INK, width=2); d.line((p, 37, p, 43), fill=INK, width=2)
        d.line((5, p, 11, p), fill=INK, width=2); d.line((37, p, 43, p), fill=INK, width=2)
    d.rectangle((23, 20, 25, 21), fill=0)       # "i"
    d.rectangle((23, 23, 25, 28), fill=0)
    return im


ICONS = {
    'stopwatch': stopwatch, 'timer': timer, 'alarm': alarm,
    'worldclock': globe, 'weather': weather, 'sysinfo': sysinfo,
}


def theme_colors(theme):
    """ink / shade colours from the theme's own settings icon."""
    pal = [c for c in lvimg.palette_of(f'spiffs_image/theme{theme}/menu/settings.bin') if c[3] > 0]
    lum = lambda c: 0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2]
    pal = sorted(set(pal), key=lum, reverse=True)
    # Themes with dark ink on a light background (theme 4) want the darkest as ink
    ink = pal[0]
    if theme == 4:
        ink = pal[-1]
    shade = pal[len(pal) // 2] if len(pal) > 2 else (pal[1] if len(pal) > 1 else ink)
    if theme != 4 and lum(shade) < 0.4 * lum(ink):
        # palette shade too dark to read on black: use a dimmed ink instead
        shade = tuple(int(c * 0.6) for c in ink[:3]) + (255,)
    return ink, shade


def render(icon, ink, shade):
    rgba = Image.new('RGBA', icon.size, (0, 0, 0, 0))
    px, src = rgba.load(), icon.load()
    for y in range(icon.height):
        for x in range(icon.width):
            v = src[x, y]
            if v == INK:
                px[x, y] = ink
            elif v == SHADE:
                px[x, y] = shade
    return rgba


def main():
    preview = None
    if '--preview' in sys.argv:
        preview = Image.new('RGBA', (len(ICONS) * (W + 4) + 4, 9 * (H + 4) + 4), (20, 20, 20, 255))
    for t in range(1, 10):
        ink, shade = theme_colors(t)
        for j, (name, fn) in enumerate(ICONS.items()):
            img = render(fn(), ink, shade)
            lvimg.encode_indexed(img, [(0, 0, 0, 0), ink, shade, (0, 0, 0, 0)], 2,
                                 f'spiffs_image/theme{t}/menu/{name}.bin')
            if preview:
                bg = Image.new('RGBA', img.size, (0, 0, 0, 255) if t != 4 else (192, 203, 220, 255))
                bg.alpha_composite(img)
                preview.paste(bg, (4 + j * (W + 4), 4 + (t - 1) * (H + 4)))
    if preview:
        preview.resize((preview.width * 3, preview.height * 3), Image.NEAREST).save(sys.argv[sys.argv.index('--preview') + 1])


if __name__ == '__main__':
    main()
