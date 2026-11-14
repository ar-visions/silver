#!/usr/bin/env python3

from pathlib import Path

import numpy as np
from PIL import Image


SIZE = 2048
ROOT = Path(__file__).parents[1]
SOURCE = ROOT / "textures" / "pluto-tiles"
OUT = ROOT / "textures" / "pluto-patches-2048"


def unit(a):
    low, high = np.percentile(a, [0.2, 99.8])
    return np.clip((a - low) / (high - low), 0.0, 1.0).astype(
        np.float32
    )


def smoothstep(a, low, high):
    t = np.clip((a - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def spectral_noise(rng, beta=2.0):
    fy = np.fft.fftfreq(SIZE).astype(np.float32)[:, None]
    fx = np.fft.rfftfreq(SIZE).astype(np.float32)[None, :]
    radius = np.sqrt(fx * fx + fy * fy)
    radius[0, 0] = 1.0
    amp = radius ** (-beta / 2.0)
    amp[0, 0] = 0.0
    phase = rng.normal(size=amp.shape) + 1j * rng.normal(
        size=amp.shape
    )
    field = np.fft.irfft2(phase * amp, s=(SIZE, SIZE))
    return unit(field)


def blur_periodic(a, sigma):
    fy = np.fft.fftfreq(a.shape[0])[:, None]
    fx = np.fft.rfftfreq(a.shape[1])[None, :]
    weight = np.exp(
        -2.0 * np.pi * np.pi * sigma * sigma * (fx * fx + fy * fy)
    )
    return np.fft.irfft2(
        np.fft.rfft2(a) * weight, s=a.shape
    ).astype(np.float32)


def tile_edges(a, width=144):
    result = a.copy()
    for i in range(width):
        amount = 0.5 + 0.5 * np.cos(np.pi * i / width)
        left = result[:, i].copy()
        right = result[:, -1 - i].copy()
        middle = (left + right) * 0.5
        result[:, i] = left * (1.0 - amount) + middle * amount
        result[:, -1 - i] = right * (1.0 - amount) + middle * amount
    for i in range(width):
        amount = 0.5 + 0.5 * np.cos(np.pi * i / width)
        top = result[i].copy()
        bottom = result[-1 - i].copy()
        middle = (top + bottom) * 0.5
        result[i] = top * (1.0 - amount) + middle * amount
        result[-1 - i] = bottom * (1.0 - amount) + middle * amount
    return result


def load_gray(path):
    image = Image.open(path).convert("L")
    image = image.resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    return np.asarray(image, dtype=np.float32) / 255.0


def load_color(path):
    image = Image.open(path).convert("RGB")
    image = image.resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    return np.asarray(image, dtype=np.float32) / 255.0


def elevation_map(name, rng):
    source = SOURCE / f"{name}_elevation.png"
    height = load_gray(source)
    fine = blur_periodic(spectral_noise(rng, 1.45), 0.8)
    height += (fine - 0.5) * 0.018
    height = tile_edges(height)
    return unit(blur_periodic(height, 0.55))


def color_map(name, height, rng, base, height_bias):
    source = SOURCE / f"{name}_texture.png"
    rgb = load_color(source)
    luma = (
        rgb[..., 0] * 0.2126
        + rgb[..., 1] * 0.7152
        + rgb[..., 2] * 0.0722
    )
    chroma = rgb / (luma[..., None] + 0.025)
    material = blur_periodic(spectral_noise(rng, 2.55), 2.0)
    target = base + (material - 0.5) * 0.13
    target += (height - 0.5) * height_bias
    color = chroma * target[..., None]
    if name == "02_fractured_ice":
        local = blur_periodic(height, 11.0)
        cracks = smoothstep(local - height, 0.022, 0.105)
        ice = np.array([0.48, 0.90, 1.00], dtype=np.float32)
        color = color * (1.0 - cracks[..., None] * 0.82)
        color += ice * cracks[..., None] * 0.82
    return np.clip(tile_edges(color), 0.0, 1.0)


def normal_map(height, strength):
    dx = np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)
    dy = np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)
    nx = -dx * strength
    ny = dy * strength
    nz = np.ones_like(height)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    normal = np.stack((nx / length, ny / length, nz / length), axis=2)
    return normal * 0.5 + 0.5


def save_maps(name, height, color, normal):
    elevation = np.round(height * 65535.0).astype(np.uint16)
    color8 = np.round(color * 255.0).astype(np.uint8)
    normal8 = np.round(normal * 255.0).astype(np.uint8)
    Image.fromarray(elevation, mode="I;16").save(
        OUT / f"{name}_elevation.png"
    )
    Image.fromarray(color8, mode="RGB").save(
        OUT / f"{name}_color.png"
    )
    Image.fromarray(normal8, mode="RGB").save(
        OUT / f"{name}_normal.png"
    )


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    palettes = [
        ("01_crater_field", 0.62, 0.035, 62.0),
        ("02_fractured_ice", 0.76, 0.025, 88.0),
        ("03_eroded_ridges", 0.60, 0.030, 76.0),
        ("04_mineral_mottling", 0.61, 0.020, 60.0),
        ("05_large_rust_blue", 0.46, 0.018, 66.0),
        ("06_ancient_nitrogen_mountains", 0.63, 0.045, 96.0),
    ]
    for index, (name, base, height_bias, strength) in enumerate(palettes):
        rng = np.random.default_rng(2015071 + index * 1009)
        height = elevation_map(name, rng)
        color = color_map(name, height, rng, base, height_bias)
        normal = normal_map(height, strength)
        save_maps(name, height, color, normal)
        print(name)


if __name__ == "__main__":
    main()
