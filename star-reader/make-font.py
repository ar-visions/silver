#!/usr/bin/env python3
"""Trace the Star Reader reference sheet into TrueType fonts."""

from collections import deque
import json
from pathlib import Path
import subprocess
import tempfile

import numpy as np
from PIL import Image
from fontTools.agl import UV2AGL
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen


HERE = Path(__file__).resolve().parent
SOURCE = HERE / "fonts/StarReader-source.png"
POTRACE = HERE.parent / "install/bin/potrace"
UNITS = 1000


def components(image, row, minimum):
    left, top, right, bottom = row
    values = np.asarray(image.crop(row).convert("L"))
    ink = values < 128
    seen = np.zeros(ink.shape, dtype=bool)
    found = []
    height, width = ink.shape
    for y in range(height):
        for x in range(width):
            if not ink[y, x] or seen[y, x]:
                continue
            todo = deque([(x, y)])
            seen[y, x] = True
            min_x = max_x = x
            min_y = max_y = y
            area = 0
            while todo:
                px, py = todo.popleft()
                area += 1
                min_x = min(min_x, px)
                max_x = max(max_x, px)
                min_y = min(min_y, py)
                max_y = max(max_y, py)
                for nx, ny in (
                    (px - 1, py),
                    (px + 1, py),
                    (px, py - 1),
                    (px, py + 1),
                ):
                    if nx < 0 or nx >= width or ny < 0 or ny >= height:
                        continue
                    if ink[ny, nx] and not seen[ny, nx]:
                        seen[ny, nx] = True
                        todo.append((nx, ny))
            if area >= minimum:
                found.append(
                    (min_x + left, min_y + top,
                     max_x + left + 1, max_y + top + 1)
                )
    return sorted(found, key=lambda box: box[0])


def trace(image, box):
    crop = image.crop(box).convert("L")
    crop = crop.point(lambda value: 0 if value < 128 else 255)
    with tempfile.TemporaryDirectory(prefix="star-reader-font-") as folder:
        bitmap = Path(folder) / "glyph.pbm"
        vector = Path(folder) / "glyph.json"
        crop.save(bitmap)
        subprocess.run(
            [
                str(POTRACE),
                "-b", "geojson",
                "--tight",
                "-t", "3",
                "-O", "0.35",
                "-o", str(vector),
                str(bitmap),
            ],
            check=True,
        )
        return json.loads(vector.read_text())


def rings(data):
    for feature in data["features"]:
        geometry = feature["geometry"]
        shapes = geometry["coordinates"]
        if geometry["type"] == "Polygon":
            shapes = [shapes]
        for shape in shapes:
            for ring in shape:
                yield ring


def traced_glyph(image, box, scale, y_offset=0, side=35):
    data = trace(image, box)
    pen = TTGlyphPen(None)
    widest = 0.0
    for ring in rings(data):
        points = ring[:-1] if ring[0] == ring[-1] else ring
        points = list(reversed(points))
        if len(points) < 3:
            continue
        mapped = [
            (round(x * scale + side), round(y * scale + y_offset))
            for x, y in points
        ]
        widest = max(widest, max(point[0] for point in mapped))
        pen.moveTo(mapped[0])
        for point in mapped[1:]:
            pen.lineTo(point)
        pen.closePath()
    advance = round(widest + side)
    return pen.glyph(), advance


def empty_glyph():
    return TTGlyphPen(None).glyph()


def glyph_name(character):
    value = ord(character)
    return UV2AGL.get(value, f"uni{value:04X}")


def make_font(path, family, characters, glyphs, metrics, space_width):
    order = [".notdef", "space"]
    order.extend(glyph_name(character) for character in characters)
    cmap = {0x20: "space"}
    cmap.update(
        {ord(character): glyph_name(character)
         for character in characters}
    )
    all_glyphs = {".notdef": empty_glyph(), "space": empty_glyph()}
    all_glyphs.update(glyphs)
    all_metrics = {".notdef": (600, 0), "space": (space_width, 0)}
    all_metrics.update(metrics)
    builder = FontBuilder(UNITS, isTTF=True)
    builder.setupGlyphOrder(order)
    builder.setupCharacterMap(cmap)
    builder.setupGlyf(all_glyphs)
    builder.setupHorizontalMetrics(all_metrics)
    builder.setupHorizontalHeader(ascent=820, descent=-220)
    builder.setupOS2(
        sTypoAscender=820,
        sTypoDescender=-220,
        usWinAscent=820,
        usWinDescent=220,
        sxHeight=520,
        sCapHeight=760,
        usWeightClass=700,
    )
    postscript = family.replace(" ", "") + "-Regular"
    builder.setupNameTable(
        {
            "familyName": family,
            "styleName": "Regular",
            "uniqueFontIdentifier": f"1.000;STAR;{postscript}",
            "fullName": family,
            "psName": postscript,
            "version": "Version 1.000",
            "copyright": "Designed for the Star Reader project.",
        }
    )
    builder.setupPost()
    builder.setupMaxp()
    builder.setupDummyDSIG()
    builder.save(path)


def title_font(image):
    capitals = components(image, (0, 270, image.width, 370), 1000)
    lowercase = components(image, (0, 410, image.width, 540), 850)
    exact = components(image, (300, 35, 1450, 210), 1000)
    if len(capitals) != 26 or len(lowercase) != 26 or len(exact) != 10:
        raise RuntimeError(
            f"title rows split as {len(capitals)}, "
            f"{len(lowercase)}, {len(exact)}"
        )
    glyphs = {}
    metrics = {}
    for character, box in zip("ABCDEFGHIJKLMNOPQRSTUVWXYZ", capitals):
        scale = 760.0 / 72.0
        offset = (355 - box[3]) * scale
        glyph, advance = traced_glyph(image, box, scale, offset)
        name = glyph_name(character)
        glyphs[name] = glyph
        metrics[name] = (advance, 35)
    for character, box in zip("abcdefghijklmnopqrstuvwxyz", lowercase):
        scale = 520.0 / 55.0
        offset = (504 - box[3]) * scale
        glyph, advance = traced_glyph(image, box, scale, offset)
        name = glyph_name(character)
        glyphs[name] = glyph
        metrics[name] = (advance, 35)
    exact_text = "StarReader"
    used = set()
    for character, box in zip(exact_text, exact):
        if character in used:
            continue
        used.add(character)
        scale = 5.0
        offset = (202 - box[3]) * scale
        glyph, advance = traced_glyph(image, box, scale, offset)
        name = glyph_name(character)
        glyphs[name] = glyph
        metrics[name] = (advance, 35)
    characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
    make_font(
        HERE / "fonts/StarReader-Title.ttf",
        "Star Reader Title",
        characters,
        glyphs,
        metrics,
        220,
    )


def math_font(image):
    numbers = components(image, (400, 580, 1370, 710), 2500)
    if len(numbers) != 10:
        raise RuntimeError(f"number row split as {len(numbers)}")
    glyphs = {}
    metrics = {}
    for character, box in zip("0123456789", numbers):
        scale = 7.0
        offset = (697 - box[3]) * scale
        glyph, advance = traced_glyph(image, box, scale, offset, 40)
        name = glyph_name(character)
        glyphs[name] = glyph
        metrics[name] = (advance, 40)
    operator_boxes = {
        "+": (545, 745, 645, 850),
        "-": (670, 775, 770, 825),
        "−": (670, 775, 770, 825),
        "×": (790, 750, 890, 850),
        "÷": (905, 745, 1010, 850),
        "=": (1035, 760, 1130, 845),
        "?": (1145, 740, 1235, 860),
    }
    for character, box in operator_boxes.items():
        scale = 7.0
        height = box[3] - box[1]
        offset = 350.0 - height * scale * 0.5
        glyph, advance = traced_glyph(image, box, scale, offset, 40)
        name = glyph_name(character)
        glyphs[name] = glyph
        metrics[name] = (advance, 40)
    characters = "0123456789+-−×÷=?"
    make_font(
        HERE / "fonts/StarReader-Math.ttf",
        "Star Reader Math",
        characters,
        glyphs,
        metrics,
        100,
    )


def main():
    image = Image.open(SOURCE).convert("RGB")
    title_font(image)
    math_font(image)


if __name__ == "__main__":
    main()
