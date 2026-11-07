"""Regenerates res/app.ico from the same signed-distance glyph the app draws at
run time (src/platform/win/tray.cpp). Usage: python scripts/make_icon.py"""
import numpy as np
from PIL import Image

ACCENT = (0x3B, 0x82, 0xF6)  # "Legion Blue" preset


def capsule(x, y, ax, ay, bx, by, r):
    pax, pay, bax, bay = x - ax, y - ay, bx - ax, by - ay
    h = np.clip((pax * bax + pay * bay) / (bax * bax + bay * bay), 0, 1)
    return np.hypot(pax - bax * h, pay - bay * h) - r


def round_box(x, y, cx, cy, hw, hh, r):
    qx, qy = np.abs(x - cx) - hw + r, np.abs(y - cy) - hh + r
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r


def glyph(x, y):
    d = round_box(x, y, 0.50, 0.68, 0.20, 0.18, 0.10)
    for fx, top in zip((0.335, 0.447, 0.559, 0.668), (0.31, 0.25, 0.28, 0.37)):
        d = np.minimum(d, capsule(x, y, fx, top, fx, 0.62, 0.036))
    d = np.minimum(d, capsule(x, y, 0.33, 0.72, 0.19, 0.56, 0.045))
    rr = np.hypot(x - 0.5, y - 0.62)
    ang = np.arctan2(y - 0.62, x - 0.5)
    ring = np.abs(rr - 0.52) - 0.034
    span = (np.abs(ang + 1.5708) - 0.62) * 0.52
    return np.minimum(d, np.maximum(ring, span))


def render(size):
    ys, xs = np.mgrid[0:size, 0:size]
    u, v = (xs + 0.5) / size, (ys + 0.5) / size
    tile_a = np.clip(0.5 - round_box(u, v, 0.5, 0.5, 0.47, 0.47, 0.22) * size, 0, 1)
    k = 1.15 - 0.3 * v
    rgb = np.stack([np.clip(c / 255 * k, 0, 1) for c in ACCENT], -1)
    s, off = 0.78, 0.11
    ga = np.clip(0.5 - glyph((u - off) / s, (v - off) / s) * size * s, 0, 1)
    rgb = rgb * (1 - ga[..., None]) + ga[..., None]
    a = np.maximum(tile_a, ga * tile_a)
    img = np.dstack([rgb, a]) * 255
    return Image.fromarray(img.round().astype(np.uint8), "RGBA")


if __name__ == "__main__":
    sizes = [16, 20, 24, 32, 40, 48, 64, 128, 256]
    big = render(256)
    big.save("res/app.ico", sizes=[(s, s) for s in sizes], append_images=[render(s) for s in sizes[:-1]])
    big.save("docs/icon.png")
    print("res/app.ico written")
