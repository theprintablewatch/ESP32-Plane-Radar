#!/usr/bin/env python3
"""Generate a LovyanGFX/TFT_eSPI .vlw anti-aliased font from a TrueType font.

The bundled data/ui_font.vlw is a 15px master, which the 800x480 build has to
upscale ~1.85x (blurry). This produces a larger native master so text renders
crisp at ~1.0x on the Waveshare 4.3B.

Usage: python scripts/gen_vlw.py <in.ttf> <out.vlw> <pixel_size> [preview.png]

VLW format (all big-endian uint32):
  header: glyphCount, version(11), fontSize, mboxY(0), ascent, descent
  per glyph (sorted by unicode): unicode, height, width, xAdvance, dY, dX, 0
  then every glyph's 8-bit alpha bitmap (height*width bytes), same order.
"""
import struct
import sys

from PIL import Image, ImageFont

# Space + printable ASCII + em dash + ellipsis (used by the UI).
CODEPOINTS = [0x20] + list(range(0x21, 0x7F)) + [0x2014, 0x2026]


def be(*vals):
    return struct.pack(">%dI" % len(vals), *[v & 0xFFFFFFFF for v in vals])


def build(ttf_path, size):
    font = ImageFont.truetype(ttf_path, size)
    ascent, descent = font.getmetrics()

    glyphs = []  # (unicode, w, h, adv, dY, dX, bitmap_bytes)
    for cp in CODEPOINTS:
        ch = chr(cp)
        try:
            mask, (ox, oy) = font.getmask2(ch, mode="L")
            w, h = mask.size
            data = bytes(mask) if w and h else b""
        except Exception:
            w = h = ox = oy = 0
            data = b""
        adv = round(font.getlength(ch))
        gdY = ascent - oy  # baseline -> top of ink (top bearing)
        gdX = ox           # left bearing
        glyphs.append((cp, w, h, adv, gdY, gdX, data))

    glyphs.sort(key=lambda g: g[0])

    out = bytearray()
    out += be(len(glyphs), 11, size, 0, ascent, descent)
    for cp, w, h, adv, gdY, gdX, _ in glyphs:
        out += be(cp, h, w, adv, gdY & 0xFFFFFFFF, gdX & 0xFFFFFFFF, 0)
    for *_, data in glyphs:
        out += data
    return bytes(out), ascent, descent, glyphs


def parse(blob):
    """Re-parse exactly like LovyanGFX does, returning metas + bitmap offsets."""
    gcount, ver, size, mboxy, asc, desc = struct.unpack(">6I", blob[:24])
    off = 24
    metas = []
    for _ in range(gcount):
        uni, h, w, adv, dY, dX, _r = struct.unpack(">7I", blob[off:off + 28])
        dY = struct.unpack(">i", blob[off + 16:off + 20])[0]
        dX = struct.unpack(">i", blob[off + 20:off + 24])[0]
        metas.append((uni, h, w, adv, dY, dX))
        off += 28
    return (gcount, ver, size, asc, desc), metas, off


def preview(blob, text, png_path):
    """Render `text` using ONLY the parsed VLW (proves the file is valid)."""
    (gcount, ver, size, asc, desc), metas, bm_off = parse(blob)
    by_uni = {}
    cur = bm_off
    for uni, h, w, adv, dY, dX in metas:
        by_uni[uni] = (h, w, adv, dY, dX, cur)
        cur += h * w

    width = sum(by_uni.get(ord(c), (0, 0, size // 2, 0, 0, 0))[2] for c in text) + 20
    height = asc + desc + 12
    img = Image.new("L", (width, height), 0)
    x = 10
    baseline = 6 + asc
    for c in text:
        m = by_uni.get(ord(c))
        if not m:
            x += size // 2
            continue
        h, w, adv, dY, dX, ptr = m
        if w and h:
            glyph = Image.frombytes("L", (w, h), blob[ptr:ptr + w * h])
            img.paste(glyph, (x + dX, baseline - dY))
        x += adv
    img.save(png_path)
    print(f"preview '{text}' -> {png_path} ({width}x{height})")


def main():
    ttf, out_path, size = sys.argv[1], sys.argv[2], int(sys.argv[3])
    png = sys.argv[4] if len(sys.argv) > 4 else None
    blob, asc, desc, glyphs = build(ttf, size)
    with open(out_path, "wb") as f:
        f.write(blob)
    print(f"wrote {out_path}: {len(glyphs)} glyphs, size={size}, "
          f"ascent={asc}, descent={desc}, {len(blob)} bytes")
    # round-trip sanity
    (gc, ver, sz, a, d), metas, _ = parse(blob)
    assert gc == len(glyphs) and ver == 11 and sz == size
    print(f"reparse OK: glyphCount={gc} version={ver} fontSize={sz} asc={a} desc={d}")
    if png:
        preview(blob, "TRAFFIC  BAW123  FL370  12 km", png)


if __name__ == "__main__":
    main()
