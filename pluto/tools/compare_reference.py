#!/usr/bin/env python3
# the rendered frame against reference/pluto-reference.png: the limb's
# rows, the sun's spot, the haze band's peaks and a grid of mean colours
# usage: compare_reference.py <render.png> [side-by-side.png]

import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).parents[1]
REF = ROOT / "reference" / "pluto-reference.png"


def limb(a, x):
    col = a[:, x]
    sky = int(np.argmax(col[:, 2] > 12))
    return sky


def mean(a, y0, y1, x0, x1):
    return tuple(a[y0:y1, x0:x1].reshape(-1, 3).mean(0).round().astype(int))


def main():
    ours = np.array(Image.open(sys.argv[1]).convert("RGB")).astype(float)
    ref = np.array(Image.open(REF).convert("RGB")).astype(float)
    if ours.shape != ref.shape:
        print("sizes differ:", ours.shape, ref.shape)
        return 1
    top = ours[:300, 900:1400].sum(2)
    y, x = np.unravel_index(top.argmax(), top.shape)
    rtop = ref[:300, 900:1400].sum(2)
    ry, rx = np.unravel_index(rtop.argmax(), rtop.shape)
    print("sun  ours (%d, %d)  ref (%d, %d)" % (x + 900, y, rx + 900, ry))
    print("limb row (haze top)  x: ours | ref")
    for xx in [20, 400, 800, 1000, 1400, 1660]:
        print("  %4d: %3d | %3d" % (xx, limb(ours, xx), limb(ref, xx)))
    print("band peak colour  x: ours | ref")
    for xx in [20, 400, 1100, 1660]:
        a = ours[150:460, xx:xx + 30].reshape(-1, 30, 3).mean(1)
        b = ref[150:460, xx:xx + 30].reshape(-1, 30, 3).mean(1)
        print("  %4d: %s | %s" % (xx,
              tuple(a[a.sum(1).argmax()].round().astype(int)),
              tuple(b[b.sum(1).argmax()].round().astype(int))))
    print("mean colour per 100 px cell (y, then x 0 400 800 1200 1600): ours | ref")
    for yy in range(350, 941, 100):
        row = []
        for xx in range(0, 1672, 400):
            row.append("%s|%s" % (mean(ours, yy, yy + 100, xx, xx + 100),
                                  mean(ref, yy, yy + 100, xx, xx + 100)))
        print("  %3d: %s" % (yy, "  ".join(row)))
    if len(sys.argv) > 2:
        r = Image.open(sys.argv[1]).convert("RGB")
        f = Image.open(REF).convert("RGB")
        w = 836
        h = int(w * f.height / f.width)
        side = Image.new("RGB", (w, h * 2))
        side.paste(f.resize((w, h)), (0, 0))
        side.paste(r.resize((w, h)), (0, h))
        side.save(sys.argv[2])
    return 0


if __name__ == "__main__":
    sys.exit(main())
