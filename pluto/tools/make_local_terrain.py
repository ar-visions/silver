#!/usr/bin/env python3

from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


ROOT = Path(__file__).parents[1]
TEXTURES = ROOT / "textures"
PATCHES = TEXTURES / "pluto-patches-2048"
OUT_A = TEXTURES / "pluto-local-albedo.png"
OUT_H = TEXTURES / "pluto-local-height.png"
OUT_N = TEXTURES / "pluto-local-normal.png"
SIDE = 2048
HEIGHT_MAX = 3.20

MASSIFS = [
    (0.24, 0.49, 0.060, 0.074, 0.16, 0.13, 2.30),
    (0.29, 0.46, 0.054, 0.067, 0.34, 0.43, 2.65),
    (0.34, 0.44, 0.058, 0.070, 0.72, 0.11, 2.95),
    (0.39, 0.41, 0.050, 0.062, 0.87, 0.20, 2.75),
    (0.44, 0.38, 0.055, 0.066, 0.56, 0.45, 2.90),
    (0.49, 0.35, 0.048, 0.058, 0.82, 0.57, 2.55),
    (0.54, 0.32, 0.052, 0.061, 0.25, 0.76, 2.45),
    (0.59, 0.29, 0.046, 0.055, 0.64, 0.76, 2.20),
    (0.64, 0.26, 0.048, 0.056, 0.16, 0.13, 2.10),
    (0.70, 0.22, 0.044, 0.052, 0.34, 0.43, 1.90),
    (0.76, 0.18, 0.042, 0.049, 0.72, 0.11, 1.70),
    (0.83, 0.14, 0.038, 0.045, 0.87, 0.20, 1.50),
]

def patch_height(stem):
    path = PATCHES / f"{stem}_elevation.png"
    return np.asarray(Image.open(path), dtype=np.float32) / 65535.0


def smoothstep(low, high, value):
    value = np.clip((value - low) / (high - low), 0.0, 1.0)
    return value * value * (3.0 - 2.0 * value)


def fractal_noise(seed):
    random = np.random.default_rng(seed)
    result = np.zeros((SIDE, SIDE), dtype=np.float32)
    total = 0.0
    for cells, weight in [(8, 1.0), (16, 0.55), (32, 0.30),
                          (64, 0.16), (128, 0.08)]:
        source = random.random((cells, cells), dtype=np.float32)
        source = np.uint8(np.rint(source * 255.0))
        image = Image.fromarray(source, "L")
        image = image.resize((SIDE, SIDE), Image.Resampling.BICUBIC)
        result += np.asarray(image, dtype=np.float32) / 255.0 * weight
        total += weight
    return np.clip(result / total, 0.0, 1.0)


def highpass_detail(source):
    image = Image.fromarray(np.uint8(np.rint(source * 255.0)), "L")
    image = image.filter(ImageFilter.GaussianBlur(22.0))
    broad = np.asarray(image, dtype=np.float32) / 255.0
    return np.clip(0.5 + (source - broad) * 3.5, 0.0, 1.0)


def stamp_massifs(source):
    source_image = Image.fromarray(
        np.uint8(np.rint(source * 255.0)), "L")
    field = np.zeros((SIDE, SIDE), dtype=np.float32)
    for tx, ty, width, height, sx, sy, peak_km in MASSIFS:
        source_radius = 230
        source_x = int(sx * SIDE)
        source_y = int(sy * SIDE)
        crop = source_image.crop((source_x - source_radius,
                                  source_y - source_radius,
                                  source_x + source_radius,
                                  source_y + source_radius))
        pixels_wide = int(width * SIDE)
        pixels_high = int(height * SIDE)
        crop = crop.resize((pixels_wide, pixels_high),
                           Image.Resampling.BICUBIC)
        stamp = np.asarray(crop, dtype=np.float32) / 255.0
        stamp = smoothstep(0.27, 0.84, stamp)
        stamp_x = np.linspace(-1.0, 1.0, pixels_wide,
                              dtype=np.float32)[None, :]
        stamp_y = np.linspace(-1.0, 1.0, pixels_high,
                              dtype=np.float32)[:, None]
        radius = stamp_x * stamp_x + stamp_y * stamp_y
        fade = smoothstep(0.0, 0.32, 1.0 - radius)
        stamp *= fade * peak_km
        left = int(tx * SIDE) - pixels_wide // 2
        top = int((ty - 0.10) * SIDE) - pixels_high // 2
        right = left + pixels_wide
        bottom = top + pixels_high
        field[top:bottom, left:right] = np.maximum(
            field[top:bottom, left:right], stamp)
    return field


def edge_fade():
    axis = np.linspace(0.0, 1.0, SIDE, dtype=np.float32)
    sides = np.minimum(axis, axis[::-1])
    sides = smoothstep(0.0, 0.025, sides)
    bottom = smoothstep(0.0, 0.030, 1.0 - axis)
    return bottom[:, None] * sides[None, :]


def save_height(height_km):
    raw = np.uint8(np.rint(np.clip(height_km / HEIGHT_MAX,
                                  0.0, 1.0) * 255.0))
    image = Image.fromarray(raw, "L")
    image = image.filter(ImageFilter.GaussianBlur(2.2))
    encoded = np.asarray(image, dtype=np.uint8)
    rgba = np.zeros((SIDE, SIDE, 4), dtype=np.uint8)
    rgba[..., :3] = encoded[..., None]
    rgba[..., 3] = 255
    Image.fromarray(rgba, "RGBA").save(OUT_H)
    return raw


def save_normal(raw):
    image = Image.fromarray(raw, "L")
    image = image.filter(ImageFilter.GaussianBlur(0.8))
    height = np.asarray(image, dtype=np.float32) / 255.0 * HEIGHT_MAX
    grad_y, grad_x = np.gradient(height)
    nx = -grad_x / (175.0 / SIDE)
    ny = grad_y / (180.0 / SIDE)
    nz = np.ones_like(nx)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    normal = np.zeros((SIDE, SIDE, 4), dtype=np.uint8)
    normal[..., 0] = np.uint8(np.rint((nx / length * 0.5 + 0.5) *
                                      255.0))
    normal[..., 1] = np.uint8(np.rint((ny / length * 0.5 + 0.5) *
                                      255.0))
    normal[..., 2] = np.uint8(np.rint((nz / length * 0.5 + 0.5) *
                                      255.0))
    normal[..., 3] = 255
    Image.fromarray(normal, "RGBA").save(OUT_N)


def build():
    axis = np.linspace(0.0, 1.0, SIDE, dtype=np.float32)
    x = axis[None, :]
    y = axis[:, None]
    rough = fractal_noise(110419)
    broad = fractal_noise(71883)
    fractured = patch_height("02_fractured_ice")
    rust_detail = patch_height("05_large_rust_blue")
    ridge_source = patch_height("03_eroded_ridges")
    ridge_source = np.roll(ridge_source, -320, axis=1)
    ridge_detail = highpass_detail(ridge_source)
    massif_source = patch_height("06_ancient_nitrogen_mountains")
    massif_source = np.roll(massif_source, -410, axis=1)

    boundary = (0.60 - 0.52 * x ** 1.10 +
                (broad - 0.5) * 0.045)
    plain = smoothstep(-0.018, 0.028, y - boundary)
    frost = np.exp(-((y - boundary) / 0.055) ** 2)

    base = (1.0 - plain) * (0.035 + rust_detail * 0.055)
    base += plain * (0.018 + fractured * 0.028)
    belt = np.exp(-((y - boundary) / 0.175) ** 2)
    belt *= smoothstep(0.07, 0.78, x)
    ridge_body = smoothstep(0.34, 0.79, ridge_source)
    mountains = ridge_body * (0.48 + ridge_source * 2.60) * belt
    massif_belt = np.exp(-((y - boundary) / 0.105) ** 2)
    massif_belt *= smoothstep(0.16, 0.84, x)
    massif_body = smoothstep(0.46, 0.82, massif_source)
    massifs = massif_body * (0.62 + massif_source * 2.15)
    mountains = np.maximum(mountains, massifs * massif_belt)
    mountains = np.maximum(mountains,
                           stamp_massifs(massif_source))
    mountain_mask = smoothstep(0.06, 0.55, mountains)
    mountains *= 0.82 + ridge_detail * 0.32
    mountains *= min(3.12 / mountains.max(), 1.0)
    height_km = np.maximum(base, mountains)
    height_km *= edge_fade()

    rust_lum = 0.70 + rust_detail * 0.47 + (rough - 0.5) * 0.12
    plain_lum = 0.88 + fractured * 0.20 + (broad - 0.5) * 0.08
    rust_color = np.array([104.0, 58.0, 44.0], dtype=np.float32)
    plain_color = np.array([158.0, 151.0, 143.0], dtype=np.float32)
    ice_color = np.array([195.0, 197.0, 199.0], dtype=np.float32)
    albedo = rust_color * rust_lum[..., None]
    albedo = albedo * (1.0 - plain[..., None])
    albedo += plain_color * plain_lum[..., None] * plain[..., None]
    frost_mix = frost * (1.0 - plain * 0.45)
    albedo = albedo * (1.0 - frost_mix[..., None] * 0.52)
    albedo += ice_color * frost_mix[..., None] * 0.52
    mountain_lum = 0.92 + rough * 0.32
    mix = mountain_mask[..., None] * 0.95
    albedo = albedo * (1.0 - mix)
    albedo += ice_color * mountain_lum[..., None] * mix
    cracks = smoothstep(0.34, 0.49, 0.58 - fractured) * plain
    albedo *= 1.0 - cracks[..., None] * 0.16
    albedo = np.uint8(np.clip(np.rint(albedo), 0, 255))
    Image.fromarray(albedo, "RGB").save(OUT_A)

    raw = save_height(height_km)
    save_normal(raw)
    print("independent local Pluto terrain written")
    print("height km: min %.3f mean %.3f max %.3f" %
          (height_km.min(), height_km.mean(), height_km.max()))


if __name__ == "__main__":
    build()
