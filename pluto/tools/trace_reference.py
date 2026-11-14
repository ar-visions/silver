#!/usr/bin/env python3
# the reference frame traced onto the ground: each pixel's class
# (tholin, plain, mountain) and its colour projected through the
# scene's camera onto the sphere, written as textures/pluto-trace.png
# and pluto-trace-color.png over a box of the virtual map (u, v as the
# shaders read it); the shader paints the traced colour where it
# covers and lays the traced class in place of the layout
# usage: trace_reference.py [preview.png]

import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).parents[1]
REF = ROOT / "reference" / "pluto-reference.png"
OUT = ROOT / "textures" / "pluto-trace.png"
OUT_C = ROOT / "textures" / "pluto-trace-color.png"
TC = 4096

W, H = 1672, 941
FOV = np.radians(40.0)
ALT = 0.0095
E_R = 1.0 + ALT
LON0 = np.radians(-27.07)
LAT0 = np.radians(28.3)
# the traced box of the virtual map, the same in pluto.ag (pl_trace_box)
BOX = (0.4265, 0.3195, 0.4500, 0.3675)
TW, TH = 1024, 1024
# past this the limb's pixels are a few km each: the layout stays
FAR_KM = 165.0
def basis():
    dip = np.arccos(1.0 / E_R)
    pit = dip + np.radians(6.45)
    f = np.array([np.cos(pit), 0.0, -np.sin(pit)])
    roll = np.radians(-6.1)
    up = np.array([0.0, np.sin(roll), np.cos(roll)])
    u = up - f * (up @ f)
    u /= np.linalg.norm(u)
    r = np.cross(f, u)
    if r[1] < 0:
        r = -r
    return f, u, r


def pixel_of(d):
    f, u, r = basis()
    tx = np.tan(FOV / 2)
    asp = W / H
    z = d @ f
    sx = (d @ r) / z / (tx * asp)
    sy = (d @ u) / z / tx
    return (sx + 1) / 2 * W - 0.5, (1 - sy) / 2 * H - 0.5, z


def frame_of_craft():
    c = np.array([np.cos(LAT0) * np.cos(LON0), np.sin(LAT0),
                  np.cos(LAT0) * np.sin(LON0)])
    e = np.array([-np.sin(LON0), 0.0, np.cos(LON0)])
    n = np.array([-np.sin(LAT0) * np.cos(LON0), np.cos(LAT0),
                  -np.sin(LAT0) * np.sin(LON0)])
    return c, e, -n


def box_filter(a, k):
    pad = k // 2
    p = np.pad(a, pad, mode="edge").astype(np.float64)
    c = np.cumsum(np.cumsum(p, 0), 1)
    c = np.pad(c, ((1, 0), (1, 0)))
    s = (c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k])
    return s / (k * k)


def classes(a):
    # tholin by its redness; mountains by their relief's contrast
    # where the ground round them is not red; the rest is the plain
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    v = np.maximum(a.max(2), 1.0)
    ratio = (r - b) / v
    lum = 0.299 * r + 0.587 * g + 0.114 * b
    red = ratio > 0.3
    var = box_filter(lum * lum, 15) - box_filter(lum, 15) ** 2
    rough = np.sqrt(np.maximum(var, 0.0)) > 19.0
    rough = box_filter(rough.astype(np.float64), 25) > 0.4
    rough &= box_filter(ratio, 25) < 0.15
    cls = np.where(red, 0, np.where(rough, 2, 1))
    return cls


def project(tw, th):
    # each texel of the box: its ground point, the reference pixel it
    # falls on (float), its distance and whether the camera sees it
    c, e, s = frame_of_craft()
    v, u = np.mgrid[0:th, 0:tw]
    uu = BOX[0] + (u + 0.5) / tw * (BOX[2] - BOX[0])
    vv = BOX[1] + (v + 0.5) / th * (BOX[3] - BOX[1])
    lon = (uu - 0.5) * 2 * np.pi
    lat = (0.5 - vv) * np.pi
    sp = np.stack([np.cos(lat) * np.cos(lon), np.sin(lat),
                   np.cos(lat) * np.sin(lon)], -1).reshape(-1, 3)
    pw = np.stack([sp @ e, sp @ s, sp @ c], -1)
    ro = np.array([0.0, 0.0, E_R])
    d = pw - ro
    t = np.linalg.norm(d, axis=1)
    d /= t[:, None]
    px, py, z = pixel_of(d)
    seen = (np.einsum("ij,ij->i", pw - ro, pw) < 0) & (z > 0)
    seen &= (t * 1188.0 < FAR_KM)
    seen &= (px >= 0) & (px < W - 1) & (py >= 0) & (py < H - 1)
    return px, py, t * 1188.0, seen


def project_rows(tc, y0, n):
    c, e, s = frame_of_craft()
    v, u = np.mgrid[y0:y0 + n, 0:tc]
    uu = BOX[0] + (u + 0.5) / tc * (BOX[2] - BOX[0])
    vv = BOX[1] + (v + 0.5) / tc * (BOX[3] - BOX[1])
    lon = (uu - 0.5) * 2 * np.pi
    lat = (0.5 - vv) * np.pi
    sp = np.stack([np.cos(lat) * np.cos(lon), np.sin(lat),
                   np.cos(lat) * np.sin(lon)], -1).reshape(-1, 3)
    pw = np.stack([sp @ e, sp @ s, sp @ c], -1)
    ro = np.array([0.0, 0.0, E_R])
    d = pw - ro
    t = np.linalg.norm(d, axis=1)
    d /= t[:, None]
    px, py, z = pixel_of(d)
    seen = (np.einsum("ij,ij->i", pw - ro, pw) < 0) & (z > 0)
    seen &= (t * 1188.0 < FAR_KM)
    seen &= (px >= 0) & (px < W - 1) & (py >= 0) & (py < H - 1)
    return px, py, t * 1188.0, seen


def bilinear(a, px, py):
    x0 = np.floor(px).astype(int)
    y0 = np.floor(py).astype(int)
    fx = (px - x0)[:, None]
    fy = (py - y0)[:, None]
    x0 = np.clip(x0, 0, W - 2)
    y0 = np.clip(y0, 0, H - 2)
    return (a[y0, x0] * (1 - fx) * (1 - fy) + a[y0, x0 + 1] * fx * (1 - fy)
            + a[y0 + 1, x0] * (1 - fx) * fy + a[y0 + 1, x0 + 1] * fx * fy)


def main():
    a = np.asarray(Image.open(REF).convert("RGB")).astype(np.float64)
    cls = classes(a)
    # the sky's edge is contrast too: no mountain within 36 rows of
    # the limb
    col = np.arange(W, dtype=np.float64)
    row = np.arange(H, dtype=np.float64)
    f, u, r = basis()
    tx = np.tan(FOV / 2)
    asp = W / H
    sx = (col + 0.5) / W * 2 - 1
    horizon = np.zeros(W, int)
    ro = np.array([0.0, 0.0, E_R])
    for x in range(W):
        sy = 1 - (row + 0.5) / H * 2
        d = f[None] + (sx[x] * tx * asp) * r[None] + sy[:, None] * tx * u[None]
        d /= np.linalg.norm(d, axis=1, keepdims=True)
        b = d @ ro
        hh = b * b - (ro @ ro - 1.0)
        horizon[x] = int(np.argmax(hh > 0))
    near_sky = row[:, None] < (horizon[None, :] + 36)
    cls = np.where(near_sky & (cls == 2), 1, cls)
    px, py, km, seen = project(TW, TH)
    ix = np.clip(px.round().astype(int), 0, W - 1)
    iy = np.clip(py.round().astype(int), 0, H - 1)
    k = cls[iy, ix]
    out = np.zeros((TH * TW, 4), np.uint8)
    out[seen & (k == 0), 0] = 255
    out[seen & (k == 1), 1] = 255
    out[seen & (k == 2), 2] = 255
    out[seen, 3] = 255
    out = out.reshape(TH, TW, 4)
    Image.fromarray(out, "RGBA").save(OUT)
    cov = seen.mean()
    print("traced %.1f%% of the box; classes tholin %d plain %d mountain %d"
          % (cov * 100, (seen & (k == 0)).sum(), (seen & (k == 1)).sum(),
             (seen & (k == 2)).sum()))
    # the colour, in row bands
    color = np.zeros((TC, TC, 4), np.uint8)
    band = 256
    for y0 in range(0, TC, band):
        px, py, km, seen = project_rows(TC, y0, band)
        rgb = bilinear(a, px, py)
        blk = np.zeros((band * TC, 4), np.float64)
        blk[:, :3] = rgb
        blk[:, 3] = 255.0 * seen
        blk[~seen, :3] = 0.0
        color[y0:y0 + band] = blk.round().astype(np.uint8).reshape(band, TC, 4)
    Image.fromarray(color, "RGBA").save(OUT_C)
    print("colour map written")
    if len(sys.argv) > 1:
        pv = np.zeros_like(a, dtype=np.uint8)
        pv[cls == 0] = (160, 50, 30)
        pv[cls == 1] = (200, 180, 150)
        pv[cls == 2] = (255, 255, 255)
        Image.fromarray(pv).save(sys.argv[1])
    return 0


if __name__ == "__main__":
    sys.exit(main())
