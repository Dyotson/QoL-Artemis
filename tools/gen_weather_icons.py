#!/usr/bin/env python3
"""Generates main/src/UIElements/WeatherIcons.{c,h}: 1-bit alpha weather icons (12 px for
the watch face, 24 px for the Weather app) that LVGL recolours to the theme colour, plus a
tiny font with just the degree sign used as a fallback for the face fonts.

Usage: gen_weather_icons.py [--preview out.png]   (run from the repo root)
"""
import math
import sys
from PIL import Image, ImageDraw, ImageFilter

ICONS = ['Unknown', 'Clear', 'ClearNight', 'PartlyCloudy', 'Cloudy', 'Fog', 'Drizzle', 'Rain', 'Snow', 'Storm', 'Wind']


def draw(name, S):
    im = Image.new('1', (S, S), 0)
    d = ImageDraw.Draw(im)
    k = S / 24.0
    P = lambda *v: [round(x * k) for x in v]
    w = max(1, round(1.6 * k))

    def sun(cx, cy, r, rays=True):
        d.ellipse(P(cx - r, cy - r, cx + r, cy + r), outline=1, width=w)
        if rays:
            for a in range(0, 360, 45):
                c, s = math.cos(math.radians(a)), math.sin(math.radians(a))
                d.line(P(cx + c * (r + 2.5), cy + s * (r + 2.5), cx + c * (r + 5), cy + s * (r + 5)), fill=1, width=w)

    def cloud(x, y, cw):
        # x,y = bottom-left; cw = width. Filled union of three puffs, then hollowed out.
        h = cw * 0.55
        mask = Image.new('L', (S, S), 0)
        md = ImageDraw.Draw(mask)
        md.ellipse(P(x, y - h * 0.6, x + cw * 0.4, y), fill=255)
        md.ellipse(P(x + cw * 0.15, y - h, x + cw * 0.65, y - h * 0.2), fill=255)
        md.ellipse(P(x + cw * 0.45, y - h * 0.8, x + cw, y), fill=255)
        md.rectangle(P(x + cw * 0.2, y - h * 0.45, x + cw * 0.8, y), fill=255)
        inner = mask.filter(ImageFilter.MinFilter(2 * w + 1))
        mp, ip, op = mask.load(), inner.load(), im.load()
        for yy in range(S):
            for xx in range(S):
                if mp[xx, yy]:
                    op[xx, yy] = 1 if not ip[xx, yy] else 0

    if name == 'Clear':
        sun(12, 12, 5)
    elif name == 'ClearNight':
        d.ellipse(P(4, 4, 20, 20), fill=1)
        d.ellipse(P(9, 1, 25, 17), fill=0)
    elif name == 'PartlyCloudy':
        sun(9, 9, 4)
        cloud(6, 21, 17)
    elif name == 'Cloudy':
        cloud(1, 19, 22)
    elif name == 'Fog':
        for y in (7, 12, 17):
            d.line(P(3, y, 21, y), fill=1, width=w)
    elif name in ('Drizzle', 'Rain', 'Snow', 'Storm'):
        cloud(1, 14, 22)
        if name == 'Drizzle':
            for x in (7, 12, 17):
                d.line(P(x, 17, x - 1, 19), fill=1, width=w)
        elif name == 'Rain':
            for x in (7, 12, 17):
                d.line(P(x, 16, x - 2, 22), fill=1, width=w)
        elif name == 'Snow':
            for x, y in ((6, 18), (12, 21), (18, 18)):
                d.rectangle(P(x - 1, y - 1, x + 1, y + 1), fill=1)
        else:
            d.line(P(13, 15, 10, 19, 14, 19, 11, 23), fill=1, width=w)
    elif name == 'Wind':
        d.line(P(2, 8, 16, 8), fill=1, width=w); d.arc(P(14, 4, 20, 10), 180, 90, fill=1, width=w)
        d.line(P(2, 13, 20, 13), fill=1, width=w)
        d.line(P(2, 18, 13, 18), fill=1, width=w); d.arc(P(11, 16, 17, 22), 270, 180, fill=1, width=w)
    else:  # Unknown: thermometer
        d.rounded_rectangle(P(10, 2, 14, 16), radius=round(2 * k), outline=1, width=w)
        d.ellipse(P(8, 14, 16, 22), fill=1)
    return im


SMALL = {
    'Clear': [
        "     #      ",
        " #   #   #  ",
        "  #     #   ",
        "    ###     ",
        "   #   #    ",
        "## #   # ## ",
        "   #   #    ",
        "    ###     ",
        "  #     #   ",
        " #   #   #  ",
        "     #      ",
        "            "],
    'ClearNight': [
        "    ###     ",
        "  ##        ",
        " ##         ",
        " #          ",
        "##          ",
        "##          ",
        "##          ",
        "##        # ",
        " ##      ## ",
        "  ###  ###  ",
        "    ####    ",
        "            "],
    'PartlyCloudy': [
        " #  #  #    ",
        "  #   #     ",
        "   ###      ",
        "## # ###    ",
        "   ##   ##  ",
        "  #       # ",
        " #         #",
        "#          #",
        "#          #",
        " ########## ",
        "            ",
        "            "],
    'Cloudy': [
        "            ",
        "            ",
        "     ###    ",
        "    #   #   ",
        "  ##     #  ",
        " #       ## ",
        "#          #",
        "#          #",
        " ########## ",
        "            ",
        "            ",
        "            "],
    'Fog': [
        "            ",
        "            ",
        " ########## ",
        "            ",
        "   #########",
        "            ",
        "##########  ",
        "            ",
        "  ######### ",
        "            ",
        "            ",
        "            "],
    'Drizzle': [
        "    ###     ",
        "  ##   ##   ",
        " #       #  ",
        "#         # ",
        "#         # ",
        " #########  ",
        "            ",
        "  #   #   # ",
        "            ",
        " #   #   #  ",
        "            ",
        "            "],
    'Rain': [
        "    ###     ",
        "  ##   ##   ",
        " #       #  ",
        "#         # ",
        "#         # ",
        " #########  ",
        "            ",
        "  #  #  #   ",
        "  #  #  #   ",
        " #  #  #    ",
        " #  #  #    ",
        "            "],
    'Snow': [
        "    ###     ",
        "  ##   ##   ",
        " #       #  ",
        "#         # ",
        "#         # ",
        " #########  ",
        "            ",
        " #   #   #  ",
        "            ",
        "   #   #    ",
        "            ",
        "            "],
    'Storm': [
        "    ###     ",
        "  ##   ##   ",
        " #       #  ",
        "#         # ",
        "#         # ",
        " ####  ###  ",
        "     ##     ",
        "    ##      ",
        "   ####     ",
        "     #      ",
        "    #       ",
        "            "],
    'Wind': [
        "            ",
        "        ##  ",
        "          # ",
        "######### # ",
        "        ##  ",
        "            ",
        "########### ",
        "            ",
        "#######  #  ",
        "        #   ",
        "         ## ",
        "            "],
    'Unknown': [
        "     #      ",
        "    # #     ",
        "    # #     ",
        "    # #     ",
        "    # #     ",
        "    ###     ",
        "   #####    ",
        "  #######   ",
        "  #######   ",
        "   #####    ",
        "            ",
        "            "],
}


def draw_small(name):
    assert SMALL[name][11].strip() == '', f'{name}: keep the bottom row empty'
    im = Image.new('1', (12, 12), 0)
    px = im.load()
    for y, row in enumerate(SMALL[name]):
        for x, c in enumerate(row[:12]):
            if c == '#':
                px[x, y] = 1
    return im


def to_c(name, im):
    w, h = im.size
    stride = (w + 7) // 8
    data = bytearray(stride * h)
    px = im.load()
    for y in range(h):
        for x in range(w):
            if px[x, y]:
                data[y * stride + x // 8] |= 0x80 >> (x % 8)
    arr = ', '.join(f'0x{b:02x}' for b in data)
    return (f'static const uint8_t {name}_map[] = {{ {arr} }};\n'
            f'const lv_img_dsc_t {name} = {{ .header = {{ .cf = LV_IMG_CF_ALPHA_1BIT, .always_zero = 0, .reserved = 0, .w = {w}, .h = {h} }}, '
            f'.data_size = {len(data)}, .data = {name}_map }};\n')


DEGREE_FONT = r'''
/* Degree sign only; used as .fallback of the face fonts (which lack it) */
static const uint8_t degree_bitmap[] = { 0x55, 0x00 };
static const lv_font_fmt_txt_glyph_dsc_t degree_glyph_dsc[] = {
	{ .bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0 },
	{ .bitmap_index = 0, .adv_w = 64, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 6 },
};
static const lv_font_fmt_txt_cmap_t degree_cmaps[] = {
	{ .range_start = 0xB0, .range_length = 1, .glyph_id_start = 1, .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY }
};
static const lv_font_fmt_txt_dsc_t degree_dsc = {
	.glyph_bitmap = degree_bitmap, .glyph_dsc = degree_glyph_dsc, .cmaps = degree_cmaps, .kern_dsc = NULL,
	.kern_scale = 0, .cmap_num = 1, .bpp = 1, .kern_classes = 0, .bitmap_format = 0, .cache = NULL
};
const lv_font_t degree_font = {
	.get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt, .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
	.line_height = 9, .base_line = 0, .subpx = LV_FONT_SUBPX_NONE, .underline_position = 1, .underline_thickness = 0,
	.dsc = &degree_dsc, .fallback = NULL, .user_data = NULL
};
'''


def main():
    c = ['// Generated by tools/gen_weather_icons.py - do not edit.', '#include "WeatherIcons.h"', '']
    for n in ICONS:
        c.append(to_c(f'wx12_{n}', draw_small(n)))
        c.append(to_c(f'wx24_{n}', draw(n, 24)))
    for size in (12, 24):
        c.append(f'const lv_img_dsc_t* const WeatherIcons{size}[] = {{ ' + ', '.join(f'&wx{size}_{n}' for n in ICONS) + ' };')
    c.append(DEGREE_FONT)
    open('main/src/UIElements/WeatherIcons.c', 'w').write('\n'.join(c) + '\n')

    h = ['// Generated by tools/gen_weather_icons.py - do not edit.', '#ifndef ARTEMIS_FIRMWARE_WEATHERICONS_H',
         '#define ARTEMIS_FIRMWARE_WEATHERICONS_H', '', '#include <lvgl.h>', '', '#ifdef __cplusplus', 'extern "C" {', '#endif', '',
         '/* Indexed by Weather::Icon */',
         'extern const lv_img_dsc_t* const WeatherIcons12[];', 'extern const lv_img_dsc_t* const WeatherIcons24[];',
         'extern const lv_font_t degree_font;', '', '#ifdef __cplusplus', '}', '#endif', '', '#endif']
    open('main/src/UIElements/WeatherIcons.h', 'w').write('\n'.join(h) + '\n')

    if '--preview' in sys.argv:
        out = Image.new('RGB', (len(ICONS) * 30 + 4, 52), (0, 0, 0))
        for i, n in enumerate(ICONS):
            out.paste(draw(n, 24).convert('RGB'), (4 + i * 30, 2))
            out.paste(draw_small(n).convert('RGB'), (4 + i * 30, 34))
        out = out.resize((out.width * 4, out.height * 4), Image.NEAREST)
        out.save(sys.argv[sys.argv.index('--preview') + 1])


if __name__ == '__main__':
    main()
