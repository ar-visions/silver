#!/usr/bin/env python3

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).parents[1]
TEXTURES = ROOT / "textures"
LAYOUT = TEXTURES / "pluto-equirectangular"
PATCHES = TEXTURES / "pluto-patches-2048"
TILES = TEXTURES / "pluto-tiles"
DEFAULT_OUT = TEXTURES / "pluto-cubemap"
HEIGHT_MAX_KM = 3.2
PLUTO_RADIUS_KM = 1188.3

FACES = {
    "px": ((1, 0, 0), (0, 0, -1), (0, -1, 0)),
    "nx": ((-1, 0, 0), (0, 0, 1), (0, -1, 0)),
    "py": ((0, 1, 0), (1, 0, 0), (0, 0, 1)),
    "ny": ((0, -1, 0), (1, 0, 0), (0, 0, -1)),
    "pz": ((0, 0, 1), (1, 0, 0), (0, -1, 0)),
    "nz": ((0, 0, -1), (-1, 0, 0), (0, -1, 0)),
}

MATERIALS = [
    "01_crater_field",
    "02_fractured_ice",
    "03_eroded_ridges",
    "04_mineral_mottling",
    "05_large_rust_blue",
    "06_ancient_nitrogen_mountains",
]

ID_COLORS = [
    (112, 112, 118),
    (190, 235, 246),
    (70, 128, 154),
    (238, 219, 160),
    (165, 55, 32),
    (230, 230, 224),
]

BASE_HEIGHT_KM = [0.12, 0.03, 0.16, 0.08, 0.10, 0.25]
RELIEF_KM = [0.44, 0.12, 1.02, 0.30, 0.54, 2.95]
SATURATION = [0.72, 0.48, 0.62, 0.72, 0.82, 0.54]
VALUE = [0.90, 1.03, 0.94, 0.90, 0.78, 1.02]
REPEAT_X = [17.0, 15.0, 13.0, 11.0, 9.0, 12.0]
REPEAT_Y = [8.5, 7.5, 6.5, 5.5, 4.5, 6.0]
PHASE = [
    (0.11, 0.37),
    (0.43, 0.19),
    (0.71, 0.53),
    (0.29, 0.83),
    (0.61, 0.07),
    (0.89, 0.67),
]


def arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--side", type=int, default=2048)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUT)
    return parser.parse_args()


def face_directions(name, side, border=True):
    count = side + 2 if border else side
    offset = -0.5 if border else 0.5
    axis = ((np.arange(count, dtype=np.float32) + offset) /
            side * 2.0 - 1.0)
    across = axis[None, :, None]
    down = axis[:, None, None]
    center, right, lower = FACES[name]
    center = np.asarray(center, dtype=np.float32)
    right = np.asarray(right, dtype=np.float32)
    lower = np.asarray(lower, dtype=np.float32)
    direction = center + across * right + down * lower
    length = np.linalg.norm(direction, axis=2, keepdims=True)
    return direction / length


def sphere_uv(direction):
    x = direction[..., 0]
    y = direction[..., 1]
    z = direction[..., 2]
    u = np.arctan2(z, x) / (2.0 * np.pi) + 0.5
    v = 0.5 - np.arcsin(np.clip(y, -1.0, 1.0)) / np.pi
    return u.astype(np.float32), v.astype(np.float32)


def sample(image, u, v, wrap_y=False):
    height, width = image.shape[:2]
    x = np.mod(u, 1.0) * width
    if wrap_y:
        y = np.mod(v, 1.0) * height
    else:
        y = np.clip(v, 0.0, 1.0) * (height - 1)
    x0 = np.floor(x).astype(np.int32) % width
    y0 = np.floor(y).astype(np.int32)
    if wrap_y:
        y0 %= height
    x1 = (x0 + 1) % width
    if wrap_y:
        y1 = (y0 + 1) % height
    else:
        y1 = np.minimum(y0 + 1, height - 1)
    fx = (x - x0).astype(np.float32)
    fy = (y - y0).astype(np.float32)
    if image.ndim == 3:
        fx = fx[..., None]
        fy = fy[..., None]
    top = image[y0, x0] * (1.0 - fx) + image[y0, x1] * fx
    bottom = image[y1, x0] * (1.0 - fx) + image[y1, x1] * fx
    return top * (1.0 - fy) + bottom * fy


def tile_edges(image, pixels=96):
    result = image.copy()
    for x in range(pixels):
        amount = (1.0 - x / pixels) ** 2
        first = result[:, x].copy()
        last = result[:, -1 - x].copy()
        middle = (first + last) * 0.5
        result[:, x] = first * (1.0 - amount) + middle * amount
        result[:, -1 - x] = last * (1.0 - amount) + middle * amount
    for y in range(pixels):
        amount = (1.0 - y / pixels) ** 2
        first = result[y].copy()
        last = result[-1 - y].copy()
        middle = (first + last) * 0.5
        result[y] = first * (1.0 - amount) + middle * amount
        result[-1 - y] = last * (1.0 - amount) + middle * amount
    result[:, -1] = result[:, 0]
    result[-1] = result[0]
    return result


def layout_masks():
    path = LAYOUT / "pluto_equirectangular_brush_ids_4096x2048.png"
    ids = np.asarray(Image.open(path).convert("RGB"))
    masks = []
    for color in ID_COLORS:
        raw = np.all(ids == np.asarray(color), axis=2).astype(np.uint8)
        tiled = np.tile(raw * 255, (1, 3))
        image = Image.fromarray(tiled, mode="L")
        image = image.filter(ImageFilter.GaussianBlur(24.0))
        width = ids.shape[1]
        blurred = np.asarray(image, dtype=np.float32)[:, width:width * 2]
        rows = 384
        for y in range(rows):
            fade = np.clip((y / rows - 0.5) * 2.0, 0.0, 1.0)
            fade = fade * fade * (3.0 - 2.0 * fade)
            amount = 1.0 - fade
            north = float(np.mean(blurred[y]))
            south = float(np.mean(blurred[-1 - y]))
            blurred[y] = blurred[y] * (1.0 - amount) + north * amount
            blurred[-1 - y] = (
                blurred[-1 - y] * (1.0 - amount) + south * amount)
        masks.append(blurred / 255.0)
    return masks


def delit_texture(path):
    image = Image.open(path).convert("RGB")
    color = np.asarray(image, dtype=np.float32) / 255.0
    luma = (color[..., 0] * 0.2126 + color[..., 1] * 0.7152 +
            color[..., 2] * 0.0722)
    luma = np.tile(np.uint8(np.rint(luma * 255.0)), (3, 3))
    broad_image = Image.fromarray(luma, mode="L")
    broad_image = broad_image.filter(ImageFilter.GaussianBlur(28.0))
    broad = np.asarray(broad_image, dtype=np.float32)
    height, width = color.shape[:2]
    broad = broad[height:height * 2, width:width * 2] / 255.0
    target = float(np.median(broad))
    gain = np.clip((target + 0.08) / (broad + 0.08), 0.72, 1.38)
    return tile_edges(np.clip(color * gain[..., None], 0.0, 1.0))


def load_materials():
    textures = []
    heights = []
    for index, name in enumerate(MATERIALS):
        path = TILES / f"{name}_texture.png"
        color = delit_texture(path)
        luma = np.sum(color * np.asarray([0.2126, 0.7152, 0.0722]),
                      axis=2, keepdims=True)
        color = luma + (color - luma) * SATURATION[index]
        textures.append(np.clip(color * VALUE[index], 0.0, 1.0))
        path = PATCHES / f"{name}_elevation.png"
        height = np.asarray(Image.open(path), dtype=np.float32) / 65535.0
        image = Image.fromarray(height, mode="F")
        image = image.resize((1024, 1024), Image.Resampling.LANCZOS)
        heights.append(np.asarray(image, dtype=np.float32))
    return textures, heights


def face_weights(masks, u, v):
    weights = np.stack([sample(mask, u, v) for mask in masks], axis=2)
    total = np.maximum(np.sum(weights, axis=2, keepdims=True), 0.0001)
    return weights / total


def material_uv(u, v, index):
    phase_u, phase_v = PHASE[index]
    return (u * REPEAT_X[index] + phase_u,
            v * REPEAT_Y[index] + phase_v)


def material_uv_alternate(u, v, index):
    phase_u, phase_v = PHASE[index]
    return (u * (REPEAT_X[index] + 4.0) + v * 3.0 + phase_v,
            u * 2.0 + v * (REPEAT_Y[index] + 1.5) + phase_u)


def smoothstep(low, high, value):
    value = np.clip((value - low) / (high - low), 0.0, 1.0)
    return value * value * (3.0 - 2.0 * value)


def render_face(name, side, masks, textures, heights):
    direction = face_directions(name, side)
    u, v = sphere_uv(direction)
    weights = face_weights(masks, u, v)
    pole = smoothstep(0.64, 0.88, np.abs(direction[..., 1]))
    shape = direction.shape[:2]
    color = np.zeros((*shape, 3), dtype=np.float32)
    height_km = np.zeros(shape, dtype=np.float32)
    for index in range(len(MATERIALS)):
        tile_u, tile_v = material_uv(u, v, index)
        alt_u, alt_v = material_uv_alternate(u, v, index)
        phase_u, phase_v = PHASE[index]
        polar_scale = REPEAT_X[index] * 0.25
        polar_u = direction[..., 0] * polar_scale + phase_u
        polar_v = direction[..., 2] * polar_scale + phase_v
        polar_alt_u = ((direction[..., 0] + direction[..., 2]) *
                       polar_scale * 0.72 + phase_v)
        polar_alt_v = ((direction[..., 2] - direction[..., 0]) *
                       polar_scale * 0.72 + phase_u)
        weight = weights[..., index]
        texture = sample(textures[index], tile_u, tile_v, True)
        alt_texture = sample(textures[index], alt_u, alt_v, True)
        texture = texture * 0.72 + alt_texture * 0.28
        polar_texture = sample(
            textures[index], polar_u, polar_v, True)
        polar_alt_texture = sample(
            textures[index], polar_alt_u, polar_alt_v, True)
        polar_texture = polar_texture * 0.72
        polar_texture += polar_alt_texture * 0.28
        texture = texture * (1.0 - pole[..., None])
        texture += polar_texture * pole[..., None]
        elevation = sample(heights[index], tile_u, tile_v, True)
        alt_elevation = sample(heights[index], alt_u, alt_v, True)
        elevation = elevation * 0.72 + alt_elevation * 0.28
        polar_elevation = sample(
            heights[index], polar_u, polar_v, True)
        polar_alt_elevation = sample(
            heights[index], polar_alt_u, polar_alt_v, True)
        polar_elevation = polar_elevation * 0.72
        polar_elevation += polar_alt_elevation * 0.28
        elevation = elevation * (1.0 - pole) + polar_elevation * pole
        color += texture * weight[..., None]
        relief = BASE_HEIGHT_KM[index] + RELIEF_KM[index] * elevation
        height_km += relief * weight
    height_km = np.clip(height_km, 0.0, HEIGHT_MAX_KM)
    return direction, color, height_km


def normal_map(direction, height_km):
    center = direction[1:-1, 1:-1]
    right = direction[1:-1, 2:]
    left = direction[1:-1, :-2]
    lower = direction[2:, 1:-1]
    upper = direction[:-2, 1:-1]
    dx_km = np.linalg.norm(right - left, axis=2) * 0.5
    dy_km = np.linalg.norm(lower - upper, axis=2) * 0.5
    dx_km *= PLUTO_RADIUS_KM
    dy_km *= PLUTO_RADIUS_KM
    dh_dx = (height_km[1:-1, 2:] -
             height_km[1:-1, :-2]) * 0.5 / dx_km
    dh_dy = (height_km[2:, 1:-1] -
             height_km[:-2, 1:-1]) * 0.5 / dy_km
    nx = -dh_dx
    ny = dh_dy
    nz = np.ones_like(nx)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    normal = np.stack((nx / length, ny / length, nz / length), axis=2)
    return np.uint8(np.rint((normal * 0.5 + 0.5) * 255.0))


def save_face(output, name, direction, color, height_km):
    texture_dir = output / "texture"
    height_dir = output / "height"
    normal_dir = output / "normal"
    for folder in (texture_dir, height_dir, normal_dir):
        folder.mkdir(parents=True, exist_ok=True)
    color = np.uint8(np.rint(np.clip(color[1:-1, 1:-1],
                                     0.0, 1.0) * 255.0))
    encoded = np.uint16(np.rint(height_km[1:-1, 1:-1] /
                                HEIGHT_MAX_KM * 65535.0))
    normal = normal_map(direction, height_km)
    Image.fromarray(color, mode="RGB").save(texture_dir / f"{name}.png")
    Image.fromarray(encoded, mode="I;16").save(height_dir / f"{name}.png")
    Image.fromarray(normal, mode="RGB").save(normal_dir / f"{name}.png")


def contact_sheet(output, kind, mode):
    thumb = 384
    margin = 28
    sheet = Image.new("RGB", (thumb * 3, (thumb + margin) * 2),
                      (16, 16, 18))
    draw = ImageDraw.Draw(sheet)
    for index, name in enumerate(FACES):
        image = Image.open(output / kind / f"{name}.png")
        if mode == "height":
            values = np.asarray(image, dtype=np.float32) / 65535.0
            image = Image.fromarray(np.uint8(np.rint(values * 255.0)),
                                    mode="L").convert("RGB")
        else:
            image = image.convert("RGB")
        image = image.resize((thumb, thumb), Image.Resampling.LANCZOS)
        x = index % 3 * thumb
        y = index // 3 * (thumb + margin) + margin
        sheet.paste(image, (x, y))
        draw.text((x + 8, y - 22), name, fill=(235, 235, 235))
    preview = output / "previews"
    preview.mkdir(parents=True, exist_ok=True)
    sheet.save(preview / f"{kind}-faces.png")


def write_manifest(output, side):
    faces = {}
    for name, (center, right, down) in FACES.items():
        faces[name] = {
            "center_direction": center,
            "image_right_direction": right,
            "image_down_direction": down,
        }
    manifest = {
        "face_size_pixels": [side, side],
        "cube_convention": "OpenGL",
        "height_encoding": {
            "format": "16-bit unsigned grayscale PNG",
            "black_km": 0.0,
            "white_km": HEIGHT_MAX_KM,
        },
        "normal_encoding": {
            "format": "8-bit RGB tangent-space OpenGL",
            "red_axis": "image right",
            "green_axis": "image up",
            "blue_axis": "surface outward",
        },
        "faces": faces,
    }
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n")


def write_readme(output, side):
    text = f"""# Pluto cube map

This folder contains six aligned texture, height and normal faces.
Every face is {side} x {side} pixels.

The face names are `px`, `nx`, `py`, `ny`, `pz` and `nz`. They use
OpenGL cube directions. `manifest.json` gives the center, right and
down direction of every image.

The height images are 16-bit grayscale PNG files. Black is 0 km and
white is {HEIGHT_MAX_KM:.1f} km above the local Pluto radius.

The normal images are OpenGL tangent-space RGB files. Red points to
the image's right, green points to the image's top, and blue points
out from Pluto's center.

The texture has no atmosphere, sunlight or cast shadow. Its global
terrain placement comes from the fixed 4096 x 2048 Pluto layout. Its
surface detail comes from the six independent Pluto terrain sources.

Run `python3 pluto/tools/make_cubemap.py` from the repository root to
rebuild the full set.
"""
    (output / "README.md").write_text(text)


def build(side, output):
    masks = layout_masks()
    textures, heights = load_materials()
    for name in FACES:
        print(f"rendering {name}", flush=True)
        direction, color, height_km = render_face(
            name, side, masks, textures, heights)
        save_face(output, name, direction, color, height_km)
    write_manifest(output, side)
    write_readme(output, side)
    contact_sheet(output, "texture", "texture")
    contact_sheet(output, "height", "height")
    contact_sheet(output, "normal", "normal")
    print(f"wrote {output}")


def main():
    args = arguments()
    if args.side < 16:
        raise ValueError("face side must be at least 16 pixels")
    build(args.side, args.output)


if __name__ == "__main__":
    main()
