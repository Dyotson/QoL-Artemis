#!/usr/bin/env python3
"""LVGL v8 .bin image <-> PNG converter for Artemis assets (indexed 1/2/4/8-bit and RGB565)."""
import struct
import sys
from PIL import Image

CF_TRUE_COLOR, CF_TRUE_COLOR_ALPHA = 4, 5
CF_INDEXED = {7: 1, 8: 2, 9: 4, 10: 8}
BPP_TO_CF = {v: k for k, v in CF_INDEXED.items()}


def decode(path):
    d = open(path, 'rb').read()
    h = struct.unpack('<I', d[:4])[0]
    cf, w, hh = h & 0x1f, (h >> 10) & 0x7ff, (h >> 21) & 0x7ff
    body = d[4:]
    img = Image.new('RGBA', (w, hh))
    px = img.load()
    if cf in CF_INDEXED:
        bpp = CF_INDEXED[cf]
        n = 1 << bpp
        pal = [(body[i * 4 + 2], body[i * 4 + 1], body[i * 4], body[i * 4 + 3]) for i in range(n)]
        data = body[n * 4:]
        stride = (w * bpp + 7) // 8
        for y in range(hh):
            row = data[y * stride:(y + 1) * stride]
            for x in range(w):
                bit = x * bpp
                v = (row[bit // 8] >> (8 - bpp - bit % 8)) & (n - 1)
                px[x, y] = pal[v]
    elif cf in (CF_TRUE_COLOR, CF_TRUE_COLOR_ALPHA):
        step = 2 if cf == CF_TRUE_COLOR else 3
        for y in range(hh):
            for x in range(w):
                o = (y * w + x) * step
                v = body[o] | (body[o + 1] << 8)  # LV_COLOR_16_SWAP may apply; handled by caller
                r, g, b = (v >> 11) << 3, ((v >> 5) & 0x3f) << 2, (v & 0x1f) << 3
                a = body[o + 2] if step == 3 else 255
                px[x, y] = (r, g, b, a)
    else:
        raise ValueError(f'unsupported cf {cf}')
    return img, cf


def palette_of(path):
    d = open(path, 'rb').read()
    cf = struct.unpack('<I', d[:4])[0] & 0x1f
    n = 1 << CF_INDEXED[cf]
    body = d[4:]
    return [(body[i * 4 + 2], body[i * 4 + 1], body[i * 4], body[i * 4 + 3]) for i in range(n)]


def encode_indexed(img, palette, bpp, out):
    """img: RGBA PIL image; palette: list of RGBA tuples (len <= 2**bpp). Nearest-colour mapping."""
    n = 1 << bpp
    pal = list(palette)[:n] + [(0, 0, 0, 0)] * (n - len(palette))
    w, h = img.size
    px = img.convert('RGBA').load()
    cache = {}

    def nearest(c):
        if c in cache:
            return cache[c]
        best = min(range(n), key=lambda i: sum((a - b) ** 2 for a, b in zip(c, pal[i])) + (0 if i < len(palette) else 10 ** 9))
        cache[c] = best
        return best

    stride = (w * bpp + 7) // 8
    data = bytearray(stride * h)
    for y in range(h):
        for x in range(w):
            v = nearest(px[x, y])
            bit = x * bpp
            data[y * stride + bit // 8] |= v << (8 - bpp - bit % 8)
    hdr = BPP_TO_CF[bpp] | (w << 10) | (h << 21)
    with open(out, 'wb') as f:
        f.write(struct.pack('<I', hdr))
        for r, g, b, a in pal:
            f.write(bytes((b, g, r, a)))
        f.write(data)


if __name__ == '__main__':
    if sys.argv[1] == 'topng':
        for p in sys.argv[2:]:
            im, cf = decode(p)
            im.save(p[:-4] + '.png') if len(sys.argv) == 3 else im.save(p.replace('/', '_')[:-4] + '.png')
            print(p, cf, im.size)
