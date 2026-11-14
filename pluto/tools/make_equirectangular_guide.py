#!/usr/bin/env python3

from pathlib import Path
import re
import subprocess
import sys

import numpy as np
from PIL import Image, ImageFilter


WIDTH = 4096
HEIGHT = 2048
ROOT = Path(__file__).parents[1]
PATCHES = ROOT / "textures" / "pluto-patches-2048"
OUT = ROOT / "textures" / "pluto-equirectangular"

BRUSHES = ["k", "c", "b", "p", "t", "m"]
PATCH_NAMES = {
    "k": "01_crater_field",
    "c": "02_fractured_ice",
    "b": "03_eroded_ridges",
    "p": "04_mineral_mottling",
    "t": "05_large_rust_blue",
    "m": "06_ancient_nitrogen_mountains",
}
ID_COLORS = {
    "k": (112, 112, 118),
    "c": (190, 235, 246),
    "b": (70, 128, 154),
    "p": (238, 219, 160),
    "t": (165, 55, 32),
    "m": (230, 230, 224),
}


def read_table():
    source = (ROOT / "pluto.ag").read_text()
    block = source.split("func pluto_table", 1)[1]
    block = block.split("\nfunc ", 1)[0]
    rows = re.findall(r"'([^']+)'", block)
    if len(rows) != 8:
        raise ValueError(f"expected 8 brush rows, found {len(rows)}")
    weights = np.zeros((8, 16, len(BRUSHES)), dtype=np.float32)
    for y, row in enumerate(rows):
        cells = row.split()
        if len(cells) != 16:
            raise ValueError(f"row {y} has {len(cells)} cells")
        for x, cell in enumerate(cells):
            parts = re.findall(r"([a-z])(\d+)", cell)
            if not parts:
                parts = [(cell, "1")]
            total = sum(int(amount) for _, amount in parts)
            for brush, amount in parts:
                if brush == "f":
                    brush = "p"
                index = BRUSHES.index(brush)
                weights[y, x, index] += int(amount) / total
    return weights


def wrapped_resize(field, size, method=Image.Resampling.BICUBIC):
    tiled = np.tile(field, (1, 3))
    image = Image.fromarray(tiled.astype(np.float32), mode="F")
    image = image.resize((size[0] * 3, size[1]), method)
    result = np.asarray(image, dtype=np.float32)
    return result[:, size[0] : size[0] * 2]


def organic_noise(rng):
    coarse = rng.random((32, 64), dtype=np.float32)
    fine = rng.random((128, 256), dtype=np.float32)
    coarse = wrapped_resize(coarse, (WIDTH, HEIGHT))
    fine = wrapped_resize(fine, (WIDTH, HEIGHT))
    return coarse * 0.72 + fine * 0.28


def brush_ids(weights):
    fields = []
    rng = np.random.default_rng(2015071)
    for index in range(len(BRUSHES)):
        field = wrapped_resize(weights[..., index], (WIDTH, HEIGHT))
        noise = organic_noise(rng)
        score = np.log(np.maximum(field, 0.000001))
        fields.append(score + (noise - 0.5) * 2.6)
    return np.argmax(np.stack(fields, axis=2), axis=2).astype(np.uint8)


def flatten_poles(image, rows=128):
    result = image.astype(np.float32)
    for y in range(rows):
        amount = (1.0 - y / rows) ** 3
        mean = result[y].mean(axis=0, keepdims=True)
        result[y] = result[y] * (1.0 - amount) + mean * amount
        mean = result[-1 - y].mean(axis=0, keepdims=True)
        result[-1 - y] = (
            result[-1 - y] * (1.0 - amount) + mean * amount
        )
    return np.rint(np.clip(result, 0.0, 255.0)).astype(np.uint8)


def blend_longitude(image, columns=96):
    result = image.astype(np.float32)
    for x in range(columns):
        amount = (1.0 - x / columns) ** 3
        left = result[:, x].copy()
        right = result[:, -1 - x].copy()
        middle = (left + right) * 0.5
        result[:, x] = left * (1.0 - amount) + middle * amount
        result[:, -1 - x] = right * (1.0 - amount) + middle * amount
    result[:, -1] = result[:, 0]
    return np.rint(np.clip(result, 0.0, 255.0)).astype(np.uint8)


def id_image(ids):
    palette = np.array(
        [ID_COLORS[brush] for brush in BRUSHES], dtype=np.uint8
    )
    result = flatten_poles(palette[ids], rows=80)
    result[:, -1] = result[:, 0]
    return result


def landscape_image(ids):
    color = np.zeros((HEIGHT, WIDTH, 3), dtype=np.float32)
    coverage = np.zeros((HEIGHT, WIDTH), dtype=np.float32)
    for index, brush in enumerate(BRUSHES):
        mask = Image.fromarray(
            np.where(ids == index, 255, 0).astype(np.uint8), mode="L"
        )
        mask = mask.filter(ImageFilter.GaussianBlur(radius=18.0))
        amount = np.asarray(mask, dtype=np.float32) / 255.0
        path = PATCHES / f"{PATCH_NAMES[brush]}_color.png"
        patch = np.asarray(Image.open(path).convert("RGB"))
        patch = np.tile(patch, (1, 2, 1)).astype(np.float32)
        color += patch * amount[..., None]
        coverage += amount
    color /= np.maximum(coverage[..., None], 0.000001)
    color = flatten_poles(color, rows=128)
    return blend_longitude(color)


def ensure_patches():
    path = PATCHES / "01_crater_field_color.png"
    if path.exists():
        return
    subprocess.run(
        [sys.executable, ROOT / "tools" / "make_patch_palettes.py"],
        check=True,
    )


def main():
    ensure_patches()
    OUT.mkdir(parents=True, exist_ok=True)
    weights = read_table()
    ids = brush_ids(weights)
    guide = landscape_image(ids)
    Image.fromarray(guide, mode="RGB").save(
        OUT / "pluto_equirectangular_brush_guide_4096x2048.png"
    )
    Image.fromarray(id_image(ids), mode="RGB").save(
        OUT / "pluto_equirectangular_brush_ids_4096x2048.png"
    )
    counts = np.bincount(ids.ravel(), minlength=len(BRUSHES))
    for brush, count in zip(BRUSHES, counts):
        share = count / ids.size * 100.0
        print(f"{brush}: {share:.1f}%")


if __name__ == "__main__":
    main()
