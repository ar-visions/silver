#!/usr/bin/env python3
"""find: where the face is in a sensor frame, how big, how much of it
the sensor is missing. one job, one script.

the input is the sensor frame sitting in the middle of a larger black
canvas. training slides the whole frame around inside that canvas and
fills the strip it vacates with a patch of the same frame taken well
away from the face, so nothing about the edge gives the answer away.

outputs, all measured in canvas fractions:
    x, y      the face box slid to its closest overlap with the sensor
    scale     the outer-corner span of the face
    obscure   the share of the face box the sensor does not hold
"""
import argparse, os, re, glob
import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from PIL import Image


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument('--session', default='top2')
    p.add_argument('--epochs',  type=int,   default=60)
    p.add_argument('--size',    type=int,   default=32)
    p.add_argument('--pad',     type=float, default=1.35)
    p.add_argument('--batch',   type=int,   default=64)
    p.add_argument('--draw',    type=int,   default=20000)
    p.add_argument('--lr',      type=float, default=0.001)
    p.add_argument('--shift',   type=float, default=0.55)
    p.add_argument('--holdout', type=float, default=10.0)
    p.add_argument('--seed',    type=int,   default=1234)
    p.add_argument('--sensor',  type=int,   default=160)
    p.add_argument('--source',  default='record',
                   choices=['record', 'render', 'both'])
    p.add_argument('--sheet',   action='store_true')
    return p.parse_args()


args = parse_args()
dev = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
NUM = r'-?[\d.]+(?:[eE][+-]?\d+)?'
SDIR = f'/src/hyperspace-sessions/{args.session}'
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models')


def read_pair(text, key):
    m = re.search(rf'{re.escape(key)}:\s*({NUM})(?:[ \t]+({NUM}))?', text)
    return (float(m.group(1)), float(m.group(2))) if m and m.group(2) else None


def load_record():
    """the hand-annotated real captures. same fields, 0..1 origin."""
    rd = os.path.join(SDIR, 'record')
    rows = []
    for agi in sorted(glob.glob(os.path.join(rd, '*.agi'))):
        text = open(agi).read()
        if 'noise: true' in text:
            continue
        fid = os.path.basename(agi)[:-4]
        for cs in re.findall(r'^ {4}(\w+):\s*\S+\.png\s*$', text, re.M):
            png = os.path.join(rd, f'{fid}-{cs}.png')
            if not os.path.exists(png):
                continue
            ocl = read_pair(text, f'{cs}_left_oc')
            ocr = read_pair(text, f'{cs}_right_oc')
            if not ocl or not ocr or min(ocl[0], ocr[0]) < -900:
                continue
            # annotations are 0..1 frame fractions, ours are centred
            ocl = (ocl[0] - 0.5, ocl[1] - 0.5)
            ocr = (ocr[0] - 0.5, ocr[1] - 0.5)
            sc = float(np.hypot(ocl[0] - ocr[0], ocl[1] - ocr[1]))
            mx = (ocl[0] + ocr[0]) * 0.5
            my = (ocl[1] + ocr[1]) * 0.5
            if sc <= 0.02 or max(abs(mx), abs(my)) + sc / 2 > 0.5:
                continue
            g = Image.open(png).convert('L')
            if g.width > args.sensor:
                g = g.resize((args.sensor, args.sensor), Image.BOX)
            rows.append(dict(fid=int(fid), cam=0,
                             g=np.asarray(g, np.float32) / 255.0,
                             mx=mx, my=my, sc=sc))
    return rows


def load_samples():
    """one row per camera view that has outer-corner labels."""
    if args.source == 'record':
        return load_record()
    rows = load_record() if args.source == 'both' else []
    for agi in sorted(glob.glob(os.path.join(SDIR, '*.agi'))):
        fid = os.path.basename(agi)[:-4]
        if not fid.isdigit():
            continue
        text = open(agi).read()
        for c in range(8):
            png = os.path.join(SDIR, f'{fid}-face{c}.png')
            if not os.path.exists(png):
                continue
            ocl = read_pair(text, f'face_left_oc{c}')
            ocr = read_pair(text, f'face_right_oc{c}')
            if not ocl or not ocr or ocl[0] < -900 or ocr[0] < -900:
                continue
            sc = float(np.hypot(ocl[0] - ocr[0], ocl[1] - ocr[1]))
            mx = (ocl[0] + ocr[0]) * 0.5
            my = (ocl[1] + ocr[1]) * 0.5
            # only faces wholly inside the frame: the augmentation
            # cannot restore pixels the camera never captured
            if sc <= 0 or max(abs(mx), abs(my)) + sc / 2 > 0.5:
                continue
            g = Image.open(png).convert('L')
            if g.width > args.sensor:
                g = g.resize((args.sensor, args.sensor), Image.BOX)
            rows.append(dict(fid=int(fid), cam=c,
                             g=np.asarray(g, np.float32) / 255.0,
                             mx=mx, my=my, sc=sc))
    if not rows:
        raise SystemExit(f'no labelled views in {SDIR}')
    return rows


def label_of(mx, my, sc):
    """plot, scale and obscure for a face at (mx, my) in the sensor."""
    P = args.pad
    lim = max(0.0, 0.5 - sc / 2)
    px = min(max(mx, -lim), lim) / P
    py = min(max(my, -lim), lim) / P
    def seen(m):
        return max(0.0, min(min(m + sc / 2, 0.5) - max(m - sc / 2, -0.5), sc))
    ob = 1.0 - seen(mx) * seen(my) / (sc * sc)
    return [px, py, sc / P, min(1.0, max(0.0, ob))]


def fill_patch(g, mx, my, sc, rng):
    """a square of this frame at least one face away from the face."""
    H, W = g.shape
    fx, fy = (mx + 0.5) * W, (my + 0.5) * H
    side = max(4, int(sc * W))
    for _ in range(24):
        x0 = rng.randint(0, max(1, W - side))
        y0 = rng.randint(0, max(1, H - side))
        if abs(x0 + side / 2 - fx) > sc * W or abs(y0 + side / 2 - fy) > sc * W:
            return g[y0:y0 + side, x0:x0 + side]
    return np.full((side, side), float(np.median(g)), np.float32)


def canvas_of(row, dx, dy, rng):
    """the frame shifted by (dx, dy) frame widths inside the canvas,
    the vacated strip filled from the frame's own background."""
    g, S, P = row['g'], args.size, args.pad
    H, W = g.shape
    C = int(round(W * P))
    pat = fill_patch(g, row['mx'], row['my'], row['sc'], rng)
    out = np.resize(pat, (C, C)).astype(np.float32)
    ox = int(round((C - W) / 2 + dx * W))
    oy = int(round((C - H) / 2 + dy * H))
    sx0, sy0 = max(0, -ox), max(0, -oy)
    dx0, dy0 = max(0, ox), max(0, oy)
    w = min(W - sx0, C - dx0)
    h = min(H - sy0, C - dy0)
    if w > 0 and h > 0:
        out[dy0:dy0 + h, dx0:dx0 + w] = g[sy0:sy0 + h, sx0:sx0 + w]
    # the sensor square stays centred: outside it is black
    keep = np.zeros((C, C), np.float32)
    lo = int(round((C - W) / 2))
    keep[lo:lo + H, lo:lo + W] = 1.0
    out = out * keep
    im = Image.fromarray((np.clip(out, 0, 1) * 255).astype(np.uint8))
    return np.asarray(im.resize((S, S), Image.BOX), np.float32)[None] / 255.0


def place(row, edge, u, other):
    """put the face where a chosen share u of it is off one edge.
    edge -1 means fully in view. the other axis stays fully in, so
    the label grades on one axis and covers 0..1 evenly."""
    sc = row['sc']
    lim = max(0.0, 0.5 - sc / 2)
    off = 0.5 + sc / 2 - sc * (1.0 - u)
    o = other * lim
    if edge < 0:
        tx, ty = u * lim, other * lim
    elif edge == 0:
        tx, ty = off, o
    elif edge == 1:
        tx, ty = -off, o
    elif edge == 2:
        tx, ty = o, off
    else:
        tx, ty = o, -off
    return tx - row['mx'], ty - row['my']


def draw(row, rng):
    # a third of the draws sit in view, the rest leave by an edge
    edge = -1 if rng.rand() < 0.34 else rng.randint(4)
    dx, dy = place(row, edge, rng.rand() * 2 - 1 if edge < 0 else rng.rand(),
                   rng.rand() * 2 - 1)
    x = canvas_of(row, dx, dy, rng)
    y = label_of(row['mx'] + dx, row['my'] + dy, row['sc'])
    return x, y


class Find(nn.Module):
    # a heat channel says where, a pooled head says how big and how
    # much is missing. both read the same trunk
    def __init__(self):
        super().__init__()
        S = args.size
        self.trunk = nn.Sequential(
            nn.Conv2d(1, 16, 5, padding=2), nn.ReLU(),
            nn.Conv2d(16, 32, 3, padding=1), nn.ReLU(),
            nn.Conv2d(32, 32, 3, padding=1), nn.ReLU())
        self.heat = nn.Conv2d(32, 1, 1)
        self.temp = nn.Parameter(torch.tensor(8.0))
        self.register_buffer('lin', torch.linspace(-0.5, 0.5, S))
        self.pool = nn.Sequential(
            nn.MaxPool2d(2), nn.Conv2d(32, 32, 3, padding=1), nn.ReLU(),
            nn.MaxPool2d(2), nn.Conv2d(32, 32, 3, padding=1), nn.ReLU(),
            nn.Flatten(), nn.Linear(32 * (S // 4) ** 2, 64), nn.ReLU(),
            nn.Linear(64, 2))

    def forward(self, x):
        t = self.trunk((x - x.mean((2, 3), keepdim=True))
                       / (x.std((2, 3), keepdim=True) + 1e-4))
        B, _, S, _ = t.shape
        p = torch.softmax(self.heat(t).reshape(B, S * S) * self.temp, 1)
        px = (p.reshape(B, S, S).sum(1) * self.lin).sum(1, keepdim=True)
        py = (p.reshape(B, S, S).sum(2) * self.lin).sum(1, keepdim=True)
        return torch.cat([px, py, self.pool(t)], 1)


def main():
    np.random.seed(args.seed)
    torch.manual_seed(args.seed)
    rows = load_samples()
    rng = np.random.RandomState(args.seed)
    ev = [r for r in rows if r['fid'] % 100 < args.holdout]
    tr = [r for r in rows if r['fid'] % 100 >= args.holdout]
    print(f'find: {len(tr)} train / {len(ev)} eval views '
          f'from {len(rows)} labelled views')

    # the eval set is fixed: every held-out view placed across the
    # whole range, so obscure is scored everywhere, not just at zero
    erng = np.random.RandomState(7)
    ex, ey = [], []
    for r in ev:
        for edge in (-1, 0, 1, 2, 3):
            for u in np.linspace(0.0, 1.0, 5):
                dx, dy = place(r, edge, u, 0.4)
                ex.append(canvas_of(r, dx, dy, erng))
                ey.append(label_of(r['mx'] + dx, r['my'] + dy, r['sc']))
    ex = torch.tensor(np.array(ex)).to(dev)
    ey = torch.tensor(np.array(ey, np.float32)).to(dev)
    obs = ey[:, 3].cpu().numpy()
    print(f'eval draws {len(ex)}: obscure {100*(obs<=.001).mean():.0f}% at 0, '
          f'{100*((obs>.001)&(obs<.999)).mean():.0f}% graded, '
          f'{100*(obs>=.999).mean():.0f}% at 1')

    if args.sheet:
        sheet(tr, rng)
        return

    m = Find().to(dev)
    opt = torch.optim.Adam(m.parameters(), lr=args.lr)
    steps = max(1, args.draw // args.batch)
    cols = ['x', 'y', 'scale', 'obscure']
    print(f'epoch = {args.draw} draws ({steps} steps)')
    print('epoch      train      eval       ' + ''.join(f'{c:<11}' for c in cols))
    best = 1e9
    for e in range(1, args.epochs + 1):
        m.train()
        tot = 0.0
        for _ in range(steps):
            pick = rng.randint(0, len(tr), args.batch)
            xb, yb = zip(*[draw(tr[i], rng) for i in pick])
            xb = torch.tensor(np.array(xb)).to(dev)
            yb = torch.tensor(np.array(yb, np.float32)).to(dev)
            loss = F.mse_loss(m(xb), yb)
            opt.zero_grad()
            loss.backward()
            opt.step()
            tot += loss.detach().item()
        m.eval()
        with torch.no_grad():
            p = m(ex)
            ee = float(F.mse_loss(p, ey))
            per = (p - ey).abs().mean(0)
        print(f'{e:3d}/{args.epochs}  {tot/steps:<10.6f} {ee:<10.6f} '
              + ''.join(f'{float(v):<11.6f}' for v in per))
        if ee < best:
            best = ee
            os.makedirs(OUT, exist_ok=True)
            torch.save(m.state_dict(), os.path.join(OUT, 'find.pt'))
            # the app's shim runs on cpu: trace there or it cannot load
            m2 = m.to('cpu').eval()
            with torch.no_grad():
                ts = torch.jit.trace(m2, torch.zeros(1, 1, args.size, args.size))
            ts.save(os.path.join(OUT, 'find.ptc'))
            m.to(dev).train()
    print(f'best eval {best:.6f} -> {OUT}/find.pt and find.ptc')


def sheet(rows, rng):
    """what the net is actually fed, with the label written under it."""
    n = 8
    tiles = []
    for r in rows[:n]:
        col = []
        for u in np.linspace(0.0, 1.0, 5):
            dx, dy = place(r, 0, u, 0.0)
            x = canvas_of(r, dx, dy, rng)[0]
            y = label_of(r['mx'] + dx, r['my'] + dy, r['sc'])
            b = np.zeros((args.size + 6, args.size), np.float32)
            b[:args.size] = x
            b[args.size + 1:args.size + 5, :] = y[3]
            col.append(b)
        tiles.append(np.concatenate(col, 0))
    im = np.concatenate(tiles, 1)
    p = '/tmp/find_sheet.png'
    Image.fromarray((np.clip(im, 0, 1) * 255).astype(np.uint8)).resize(
        (im.shape[1] * 5, im.shape[0] * 5), Image.NEAREST).save(p)
    print(f'wrote {p} — columns are one view at five shifts, the bar '
          f'under each tile is its obscure label')


if __name__ == '__main__':
    main()
