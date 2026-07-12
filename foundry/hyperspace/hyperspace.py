#!/usr/bin/env python3
# PyTorch look trainer — standalone (no tensorflow anywhere).
# Center-anchored gaze net: the aux geometry (eye centers + scale in
# the camera frame) drives a supervised head-center anchor and
# modulates the eye features; gaze = anchor + delta.
import argparse, os, re
import numpy as np
import torch
import torch.nn as nn


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument('--session',   default='a7')
    p.add_argument('--epochs',    type=int,   default=30)
    p.add_argument('--optimizer', default='adam', choices=['adam', 'sgd'])
    p.add_argument('--lr',        type=float, default=0.001)
    p.add_argument('--batch',     type=int,   default=1)   # batch > 1 is faster, never better
    p.add_argument('--train',     action='store_true')   # parity with silver CLI
    p.add_argument('--size',      type=int,   default=32)
    p.add_argument('--cams',      type=int,   default=1)
    p.add_argument('--seed',      type=int,   default=1234)
    p.add_argument('--zero_aux',  type=int,   default=0)
    p.add_argument('--dropout',   type=float, default=0.000)
    p.add_argument('--eps',       type=float, default=0.005)  # error tolerance, screen units
    p.add_argument('--mirror',    type=int,   default=0)     # 1 = mirror half of each batch
    p.add_argument('--wd',        type=float, default=1e-4)   # weight decay (AdamW)
    p.add_argument('--noise',     type=float, default=0.00)   # train pixel noise
    p.add_argument('--photo',     type=float, default=0.10)   # contrast/brightness jitter, +/- fraction
    p.add_argument('--eye_div',   type=float, default=2.12)   # eye crop side = scale / eye_div (2x area vs /3)
    p.add_argument('--taug',   type=int,   default=0)     # fake-translation variants per sample
    p.add_argument('--trange', type=float, default=0.04)   # max |head shift|, frame units
    p.add_argument('--tkx',    type=float, default=1.0)   # screen shift = tk * shift / scale
    p.add_argument('--tky',    type=float, default=1.0)
    p.add_argument('--saug',   type=int,   default=0)     # fake-depth variants per sample
    p.add_argument('--sspan',  type=float, default=1.8)   # scale factor range [1/sspan, sspan]
    p.add_argument('--ekx',    type=float, default=0.078) # eye->screen proj, x (6.3cm/81cm)
    p.add_argument('--eky',    type=float, default=0.137) # eye->screen proj, y (6.3cm/46cm)
    p.add_argument('--ey0',    type=float, default=-0.5) # camera axis screen y (-0.5 = top)
    p.add_argument('--fraction',  type=float, default=1.0)   # train on this fraction of frames
    p.add_argument('--volume',    type=int,   default=1)     # ingest volume_look/volume_head folders
    p.add_argument('--vel_tol',   type=float, default=0.02)  # max detection deviation vs smoothed track
    p.add_argument('--rwin',      type=int,   default=4)     # refine: jittered windows per frame
    p.add_argument('--balance',   type=int,   default=1)     # look: equalize screen-gaze bin sampling
    p.add_argument('--bins_x',    type=int,   default=16)    # balance grid: 16x9 screen areas
    p.add_argument('--bins_y',    type=int,   default=9)
    p.add_argument('--annotate',  action='store_true')      # plot eyes on unannotated frames (torch models)
    p.add_argument('--process',   default='', choices=['', 'base', 'volumes', 'gen',
                                                       'look', 'target', 'refine', 'noise'])
    p.add_argument('--export_ts', action='store_true')      # trace models -> app .ptc files
    p.add_argument('--preview',   action='store_true')      # dump look training samples as pngs
    p.add_argument('--stats',     action='store_true')      # dataset variety / coverage report
    p.add_argument('--oc_span_m', type=float, default=0.117) # measured outer-corner span: the unit truth (4.6in)
    p.add_argument('--group_by',  default='frame', choices=['circle', 'pose', 'headdir', 'frame'])  # eval holdout unit
    return p.parse_args()

args = parse_args()
args.model = 'look' if args.process in ('', 'look', 'gen', 'base', 'volumes') else args.process


def read_pair(agi_text, key):
    # scale entries carry a single value; y is optional
    m = re.search(rf'{re.escape(key)}:\s*(-?[\d.]+)(?:[ \t]+(-?[\d.]+))?', agi_text)
    if not m:
        return (-1.0, -1.0)
    return (float(m.group(1)), float(m.group(2)) if m.group(2) else 0.0)


def oc_or_extrap(text, left, right):
    # outer corners in GLOBAL camera coords; frames not yet corner-
    # plotted extrapolate along the eye line (~35% spacing outward)
    ocl = read_pair(text, 'top_left_oc')
    ocr = read_pair(text, 'top_right_oc')
    if ocl[0] < -0.5 or ocr[0] < -0.5:
        dx9, dy9 = left[0] - right[0], left[1] - right[1]
        ocl = (left[0] + 0.35 * dx9, left[1] + 0.35 * dy9)
        ocr = (right[0] - 0.35 * dx9, right[1] - 0.35 * dy9)
    return ocl, ocr


def pupil_geometry(session_dir, cam='top'):
    # pupil-zero groups: two zero-frame clicks + one end-of-group
    # drift click fix eye centers for EVERY member; drift lerps
    # 0 -> measured across capture order. scale = span, floor 0.15
    zeros, members, drifts = {}, {}, {}
    for i in agi_ids(session_dir):
        text = open(os.path.join(session_dir, f'{i}.agi')).read()
        if 'zero: true' in text:
            pl = read_pair(text, f'{cam}_pupil_left')
            pr = read_pair(text, f'{cam}_pupil_right')
            if pl[0] > -0.5 and pr[0] > -0.5:
                zeros[i] = (pl, pr)
        m = re.search(r'^sequence:\s*(\d+)', text, re.M)
        if m:
            z = int(m.group(1))
            members.setdefault(z, []).append(i)
            d = read_pair(text, f'{cam}_drift')
            if d != (-1.0, -1.0):
                drifts.setdefault(z, {})[i] = d
    geo = {}
    # ungrouped head-focused frames (edges laps): their two clicks
    # are a complete single-frame geometry
    for i in agi_ids(session_dir):
        text = open(os.path.join(session_dir, f'{i}.agi')).read()
        if ('zero: true' not in text
                and not re.search(r'^sequence:\s*\d+', text, re.M)):
            pl = read_pair(text, f'{cam}_pupil_left')
            pr = read_pair(text, f'{cam}_pupil_right')
            if pl[0] > -0.5 and pr[0] > -0.5:
                span = ((pl[0] - pr[0]) ** 2 + (pl[1] - pr[1]) ** 2) ** 0.5
                geo[i] = (pl, pr, span)
    for z, (pl, pr) in zeros.items():
        mem = sorted(members.get(z, []))
        # anchors: zero frame is drift 0; each annotated frame is its
        # own correction; drift between anchors is piecewise-linear,
        # held flat past the last one (drift is NOT globally linear)
        anchors = [(z, (0.0, 0.0))] + sorted(drifts.get(z, {}).items())
        for fid in [z] + mem:
            prev, nxt = anchors[0], None
            for a in anchors:
                if a[0] <= fid:
                    prev = a
                else:
                    nxt = a
                    break
            if nxt is None:
                dd = prev[1]
            else:
                w = (fid - prev[0]) / (nxt[0] - prev[0])
                dd = (prev[1][0] + (nxt[1][0] - prev[1][0]) * w,
                      prev[1][1] + (nxt[1][1] - prev[1][1]) * w)
            l9 = (pl[0] + dd[0], pl[1] + dd[1])
            r9 = (pr[0] + dd[0], pr[1] + dd[1])
            span = ((l9[0] - r9[0]) ** 2 + (l9[1] - r9[1]) ** 2) ** 0.5
            geo[fid] = (l9, r9, span)
    return geo


def agi_ids(d):
    # root frame ids are no longer contiguous (edge sweeps moved to
    # edges/) — enumerate what exists instead of walking until a gap
    if not os.path.isdir(d):
        return []
    return sorted(int(f[:-4]) for f in os.listdir(d)
                  if f.endswith('.agi') and f[:-4].isdigit())


def noise_dir(sd):
    # off-camera negatives live in noise/ (volume_off = legacy name)
    nd = os.path.join(sd, 'noise')
    return nd if os.path.isdir(nd) else os.path.join(sd, 'volume_off')


def eyes_off_frame(left, right):
    # look trains ONLY on frames with both eyes fully on screen — any
    # off-frame plot means unreliable crops and it is excluded
    def off(p):
        return p[0] < 0.0 or p[0] > 1.0 or p[1] < 0.0 or p[1] > 1.0
    return off(left) or off(right)


def crop_tensor(gray, cx, cy, side):
    # square crop -> size x size gray, coverage-averaged (vectorized;
    # edge-clamped like silver crop_tensor)
    h, w = gray.shape
    img_w = float(w)
    size = args.size
    side_px = side * img_w if side > 0.0 else img_w
    x0 = cx * img_w - side_px * 0.5 if side > 0.0 else 0.0
    y0 = cy * img_w - side_px * 0.5 if side > 0.0 else 0.0
    step = side_px / size
    ns = int(min(max(step, 1) + 2, 12))
    o = (np.arange(ns) + 0.5) / ns * step
    xs = np.clip((x0 + (np.arange(size) * step)[:, None] + o[None, :])
                 .reshape(-1).astype(int), 0, w - 1)
    ys = np.clip((y0 + (np.arange(size) * step)[:, None] + o[None, :])
                 .reshape(-1).astype(int), 0, h - 1)
    vals = gray.astype(np.float32)[np.ix_(ys, xs)]
    out = vals.reshape(size, ns, size, ns).mean(axis=(1, 3))
    return (out / 255.0).reshape(size, size, 1)


def crop_rect(gray, cx, cy, side_w, side_h):
    # rectangular crop (both sides in image-width units) -> SxS,
    # anisotropic coverage-average; edge-clamped like crop_tensor
    h, w = gray.shape
    img_w = float(w)
    size = args.size
    swp, shp = side_w * img_w, side_h * img_w
    x0 = cx * img_w - swp * 0.5
    y0 = cy * img_w - shp * 0.5
    stx, sty = swp / size, shp / size
    nsx = int(min(max(stx, 1) + 2, 12))
    nsy = int(min(max(sty, 1) + 2, 12))
    oxs = (np.arange(nsx) + 0.5) / nsx * stx
    oys = (np.arange(nsy) + 0.5) / nsy * sty
    xs = np.clip((x0 + (np.arange(size) * stx)[:, None] + oxs[None, :])
                 .reshape(-1).astype(int), 0, w - 1)
    ys = np.clip((y0 + (np.arange(size) * sty)[:, None] + oys[None, :])
                 .reshape(-1).astype(int), 0, h - 1)
    vals = gray.astype(np.float32)[np.ix_(ys, xs)]
    out = vals.reshape(size, nsy, size, nsx).mean(axis=(1, 3))
    return (out / 255.0).reshape(size, size, 1)


def load_session(session_dir):
    # the trainer builds NO crops. silver's build_inputs (the exact
    # runtime function) writes crops/ via --export_crops; we load it.
    cd = os.path.join(session_dir, 'crops')
    idx = os.path.join(cd, 'index.txt')
    if not os.path.exists(idx):
        raise SystemExit('crops/index.txt missing — run: silver hyperspace --export_crops')
    cache = os.path.join(session_dir, f'.torchcache-look-v29.npz')
    if os.path.exists(cache) and os.path.getmtime(cache) >= os.path.getmtime(idx):
        z = np.load(cache)
        return [z[k] for k in ('tl', 'tr', 'tf')], z['laux'], z['ly'], z['lg'], z['lfid']
    from PIL import Image
    print('loading exported crops ...')
    look = {'tl': [], 'tr': [], 'tf': []}
    laux, ly, lg, lfid = [], [], [], []
    groups = {}
    for line in open(idx):
        parts = line.split()
        if len(parts) != 13:
            continue
        fid = int(parts[0])
        text = open(os.path.join(session_dir, f'{fid}.agi')).read()
        center = read_pair(text, 'head')
        lkm    = read_pair(text, 'look')
        if center[0] <= -0.9 or lkm[0] <= -0.9:
            continue
        for key, tag in (('tl', 'l'), ('tr', 'r'), ('tf', 'f')):
            px = np.asarray(Image.open(os.path.join(cd, f'{fid}-{tag}.png')),
                            np.float32) / 255.0
            look[key].append(px.reshape(args.size, args.size, 1))
        laux.append([float(v) for v in parts[1:13]])
        ly.append([center[0], center[1], lkm[0], lkm[1]])
        gk = (round(center[0], 5), round(center[1], 5))
        lg.append(groups.setdefault(gk, len(groups)))
        lfid.append(fid)
    r = ([np.array(look[k], np.float32) for k in ('tl', 'tr', 'tf')],
         np.array(laux, np.float32), np.array(ly, np.float32),
         np.array(lg), np.array(lfid))
    np.savez_compressed(cache, tl=r[0][0], tr=r[0][1], tf=r[0][2],
                        laux=r[1], ly=r[2], lg=r[3], lfid=r[4])
    return r


def coord_grid(S):
    # CoordConv: constant x/y ramps — conv features become position-aware,
    # because the translation AMOUNT is the signal in this task
    ys, xs = torch.meshgrid(torch.linspace(-1, 1, S),
                            torch.linspace(-1, 1, S), indexing='ij')
    return torch.stack([xs, ys])          # (2,S,S)


class EyeEnc(nn.Module):
    # hybrid: sub-pixel soft-argmax coordinates (no pooling on that
    # path — a 1px iris shift moves them directly) PLUS pooled
    # appearance features for context. 24 + 64 = 88 per eye.
    def __init__(self, k=4):
        super().__init__()
        self.k = k
        self.trunk = nn.Sequential(
            nn.Conv2d(3, 4, 5, padding=2), nn.GroupNorm(1, 4), nn.ReLU(),
            nn.Conv2d(4, 8, 5, padding=2), nn.GroupNorm(2, 8), nn.ReLU(),
            nn.Conv2d(8, 8, 5, padding=2), nn.GroupNorm(2, 8), nn.ReLU())
        self.register_buffer('cgrid', coord_grid(args.size))
        self.heat = nn.Conv2d(8, k, 1)
        self.temp = nn.Parameter(torch.tensor(8.0))
        s = args.size // 4
        self.app = nn.Sequential(
            nn.MaxPool2d(2),
            nn.Conv2d(8, 8, 5, padding=2), nn.GroupNorm(2, 8), nn.ReLU(),
            nn.MaxPool2d(2), nn.MaxPool2d(2),
            nn.Flatten())
        lin = torch.linspace(0.0, 1.0, args.size)
        self.register_buffer('lin', lin)

    def forward(self, x):
        g = self.cgrid.unsqueeze(0).expand(x.shape[0], -1, -1, -1)
        t = self.trunk(torch.cat([x, g], 1))
        hm = self.heat(t)                      # B,k,S,S
        B, k, S, _ = hm.shape
        p = torch.softmax(hm.reshape(B, k, -1) * self.temp, -1).reshape(B, k, S, S)
        xs = (p.sum(2) * self.lin).sum(-1)     # B,k sub-pixel x
        ys = (p.sum(3) * self.lin).sum(-1)     # B,k sub-pixel y
        pk = hm.reshape(B, k, -1).max(-1).values * 0.1   # confidence
        return torch.cat([xs, ys, pk, self.app(t)], 1)   # B, 3k+64


class CenterNet(nn.Module):
    def __init__(self):
        super().__init__()
        # aux joins LATE: image features run their dense first, the
        # raw aux concatenates just before each head's final dense
        # ONE encoder design everywhere: eyes, face, scene
        # three SEPARATE CNN columns (left eye, right eye, face);
        # each column's dense gets its own copy of the aux, and the
        # join gets the aux AGAIN. head reads the face column only.
        self.eyeL = EyeEnc()
        self.eyeR = EyeEnc()
        self.face = EyeEnc()
        fe = 3 * 4 + 128                       # encoder output width
        self.colL = nn.Sequential(nn.Linear(fe + 12, 64), nn.ReLU())
        self.colR = nn.Sequential(nn.Linear(fe + 12, 64), nn.ReLU())
        self.colF = nn.Sequential(nn.Linear(fe + 12, 64), nn.ReLU())
        self.center = nn.Sequential(nn.Linear(64 + 12, 32), nn.ReLU(),
                                    nn.Dropout(args.dropout),
                                    nn.Linear(32, 2))
        self.delta = nn.Sequential(nn.Linear(64 * 3 + 12 + 2, 64), nn.ReLU(),
                                   nn.Dropout(args.dropout),
                                   nn.Linear(64, 2))

    def forward(self, l, r, f, a):
        hl = self.colL(torch.cat([self.eyeL(l), a], 1))
        hr = self.colR(torch.cat([self.eyeR(r), a], 1))
        hf = self.colF(torch.cat([self.face(f), a], 1))
        center = self.center(torch.cat([hf, a], 1))
        # the face direction itself feeds the gaze stage (detached:
        # the head stays trained by the face alone)
        delta  = self.delta(torch.cat([hl, hr, hf, a, center.detach()], 1))
        return center, center + delta


def pad(txt, w=13):
    return str(txt)[:w - 1].ljust(w)


def nchw(x):
    return torch.from_numpy(np.ascontiguousarray(x.transpose(0, 3, 1, 2)))


def load_plot_frames(session_dir):
    # target/refine base set: gold main-dir labels + volume folders with
    # median-smoothed auto-annotations (velocity outliers dropped)
    from PIL import Image
    entries = []
    geo = {cam: pupil_geometry(session_dir, cam) for cam in ('top', 'bot')}
    for i in agi_ids(session_dir):
        text = open(os.path.join(session_dir, f'{i}.agi')).read()
        # every camera's view is an independent plot sample
        for cam in ('top', 'bot'):
            left  = read_pair(text, f'{cam}_left')
            right = read_pair(text, f'{cam}_right')
            scale = read_pair(text, f'{cam}_scale')
            png = os.path.join(session_dir, f'{i}-{cam}.png')
            g9 = geo[cam].get(i)
            # edge frames carry the extreme poses the detectors die
            # on: they weigh 32x for BOTH target and refine
            rep = 32 if 'edges: true' in text else 1
            if g9 is not None and os.path.exists(png):
                l9, r9, s9 = g9
                for _ in range(rep):
                    entries.append((png, [l9[0], l9[1], r9[0], r9[1], s9]))
            elif left[0] > -900 and right[0] > -900 and scale[0] > 0 and os.path.exists(png):
                for _ in range(rep):
                    entries.append((png, [left[0], left[1], right[0], right[1], scale[0]]))
    n_gold = len(entries)
    for sub in ('look', 'head'):
        vd = os.path.join(session_dir, 'volume_' + sub)
        ad = os.path.join(session_dir, 'applied_' + sub)
        seq = []
        i = 0
        while os.path.exists(os.path.join(ad, f'{i}.agi')):
            text = open(os.path.join(ad, f'{i}.agi')).read()
            left, right = read_pair(text, 'top_left'), read_pair(text, 'top_right')
            scale = read_pair(text, 'top_scale')
            png = os.path.join(vd, f'{i}-top.png')
            if left[0] > -900 and right[0] > -900 and scale[0] > 0 and os.path.exists(png):
                seq.append((png, [left[0], left[1], right[0], right[1], scale[0]]))
            i += 1
        if len(seq) < 5:
            continue
        lab = np.array([s[1] for s in seq], np.float32)
        sm = lab.copy()
        for j in range(len(lab)):
            a = max(0, j - 2)
            sm[j] = np.median(lab[a:a + 5], axis=0)
        ok = np.abs(lab[:, :4] - sm[:, :4]).max(1) < args.vel_tol
        # labels REPLACED by the smoothed track: detector noise averages
        # out over a slow-moving head
        for j, s in enumerate(seq):
            if ok[j]:
                entries.append((s[0], sm[j].tolist()))
        print(f'  plots from volume_{sub}: {int(ok.sum())} kept / {len(seq)} ({int((~ok).sum())} velocity-rejected)')
    print(f'plot frames: {len(entries)} total ({n_gold} gold)')
    return entries


def build_plot_cache(session_dir):
    # --session accepts a comma list: every rig's data keeps feeding
    # the camera-space base models
    if ',' in args.session:
        entries = []
        for s in args.session.split(','):
            entries += load_plot_frames(f'/src/hyperspace-sessions/{s}')
        return _plot_arrays(entries)
    tag = args.model
    cache = os.path.join(session_dir, f'.torchcache-{tag}-{args.size}-w{args.rwin}-v22.npz')
    dirs = [session_dir] + [os.path.join(session_dir, s)
                            for s in ('volume_look', 'volume_head',
                                      'applied_look', 'applied_head')
                            if os.path.isdir(os.path.join(session_dir, s))]
    newest = max((os.path.getmtime(os.path.join(d, f))
                  for d in dirs for f in os.listdir(d) if f.endswith('.agi')), default=0)
    if os.path.exists(cache) and os.path.getmtime(cache) >= newest:
        z = np.load(cache)
        return z['x'], z['y']
    from PIL import Image
    print(f'building {tag} cache ...')
    entries = load_plot_frames(session_dir)
    x, y = _plot_arrays(entries)
    np.savez_compressed(cache, x=x, y=y)
    return x, y


def _patch_sources(session_dir):
    # organic patch images recorded into {session}/patch/
    from PIL import Image
    import glob as g2
    out = []
    for f in sorted(g2.glob(os.path.join(session_dir, 'patch', '*-top.png'))):
        out.append(np.asarray(Image.open(f).convert('L'), np.float32) / 255.0)
    return out


def _patch_blobs(t, src, wl, thr=0.62, mid_guard=0.0):
    # smooth noise blobs of eye-free organic imagery cover parts of
    # the window — sections picked at one of 16 scales; one eye
    # always stays visible. thr raises = smaller blobs; mid_guard
    # keeps a clear disc around the eye midpoint (face protection)
    from PIL import Image
    S = t.shape[0]
    # noise grid 2..8 cells: big and small blob scales; edges FEATHER
    # most of the time (wide soft band), hard only 1 in 5
    gn = int(np.random.choice([2, 3, 4, 6, 8]))
    g8 = (np.random.rand(gn, gn) * 255).astype(np.uint8)
    ge = np.asarray(Image.fromarray(g8).resize((S, S), Image.BILINEAR),
                    np.float32) / 255.0
    alpha = (np.clip((ge - thr + 0.07) / 0.3, 0.0, 1.0)
             if np.random.rand() < 0.8 else None)
    mask = ge > thr
    if mid_guard > 0.0:
        gx = (wl[0] + wl[2]) * 0.5
        gy = (wl[1] + wl[3]) * 0.5
        yy, xx = np.ogrid[:S, :S]
        d2g = (xx - gx * (S - 1)) ** 2 + (yy - gy * (S - 1)) ** 2
        keepg = d2g > (S * mid_guard) ** 2
        mask &= keepg
        if alpha is not None:
            alpha = alpha * keepg
    if not mask.any():
        return t
    def covered(x, y):
        px, py = int(x * (S - 1)), int(y * (S - 1))
        return mask[max(py - 1, 0):py + 2, max(px - 1, 0):px + 2].any()
    eyes = [(wl[0], wl[1]), (wl[2], wl[3])]
    ins = [e for e in eyes if 0.0 <= e[0] <= 1.0 and 0.0 <= e[1] <= 1.0]
    if len(ins) == 2 and covered(*ins[0]) and covered(*ins[1]):
        ex, ey = ins[np.random.randint(2)]
        yy, xx = np.ogrid[:S, :S]
        d2 = (xx - ex * (S - 1)) ** 2 + (yy - ey * (S - 1)) ** 2
        keep = d2 > (S * 0.18) ** 2
        mask &= keep
        if alpha is not None:
            alpha = alpha * keep
    h, w = src.shape
    lvl = np.random.randint(16)
    side = int(S + (min(h, w) - 1 - S) * lvl / 15)
    y0 = np.random.randint(0, h - side + 1)
    x0 = np.random.randint(0, w - side + 1)
    sec = src[y0:y0 + side, x0:x0 + side]
    if side != S:
        sec = np.asarray(Image.fromarray((sec * 255).astype(np.uint8))
                         .resize((S, S)), np.float32) / 255.0
    t = t.copy()
    if alpha is not None:
        t[:, :, 0] = t[:, :, 0] * (1.0 - alpha) + sec * alpha
    else:
        t[:, :, 0][mask] = sec[mask]
    return t


def _plot_arrays(entries):
    from PIL import Image
    xs, ys = [], []
    for png, lab in entries:
        gray = np.asarray(Image.open(png).convert('L'))
        if args.model == 'target':
            xs.append(crop_tensor(gray, 0.0, 0.0, 0.0))
            ys.append(lab)
            # 7x: +/-30% zoom BOTH ways (distance AND close-up, where
            # a big face clips the frame) then shifts up to HALF the
            # screen — eyes stay supervised on partial faces
            h2, w2 = gray.shape
            for _ in range(6):
                f9 = 0.7 + np.random.rand() * 0.6
                if f9 < 0.98:
                    rw, rh = max(2, int(w2 * f9)), max(2, int(h2 * f9))
                    small = np.asarray(Image.fromarray(gray).resize((rw, rh)))
                    zg = np.zeros_like(gray)
                    x0z, y0z = (w2 - rw) // 2, (h2 - rh) // 2
                    zg[y0z:y0z + rh, x0z:x0z + rw] = small
                elif f9 > 1.02:
                    rw, rh = int(w2 * f9), int(h2 * f9)
                    big = np.asarray(Image.fromarray(gray).resize((rw, rh)))
                    x0z, y0z = (rw - w2) // 2, (rh - h2) // 2
                    zg = big[y0z:y0z + h2, x0z:x0z + w2]
                else:
                    zg = gray
                if abs(f9 - 1.0) > 0.02:
                    lab9 = [0.5 + (lab[0] - 0.5) * f9, 0.5 + (lab[1] - 0.5) * f9,
                            0.5 + (lab[2] - 0.5) * f9, 0.5 + (lab[3] - 0.5) * f9,
                            lab[4] * f9]
                else:
                    lab9 = list(lab)
                sx = int((np.random.rand() - 0.5) * 1.0 * w2)
                sy = int((np.random.rand() - 0.5) * 1.0 * w2)
                sh2 = np.zeros_like(zg)
                x0s, x1s = max(0, sx), min(w2, w2 + sx)
                y0s, y1s = max(0, sy), min(h2, h2 + sy)
                sh2[y0s:y1s, x0s:x1s] = zg[y0s - sy:y1s - sy, x0s - sx:x1s - sx]
                xs.append(crop_tensor(sh2, 0.0, 0.0, 0.0))
                dx, dy = sx / w2, sy / w2
                # centroid within half a face-scale of an edge: the
                # face is effectively leaving the view -> off_screen
                cxs = (lab9[0] + lab9[2]) / 2 + dx
                cys = (lab9[1] + lab9[3]) / 2 + dy
                hs = lab9[4] / 2
                offl = 1.0 if (cxs < hs or cxs > 1 - hs
                               or cys < hs or cys > 1 - hs) else 0.0
                ys.append([lab9[0] + dx, lab9[1] + dy,
                           lab9[2] + dx, lab9[3] + dy, lab9[4], offl])
        else:
            # crop centers on the median eye, x/y offset +/-35% of plot scale
            lx, lyv, rx, ry, sc = lab
            mx = (lx + rx) / 2
            my = (lyv + ry) / 2
            for _ in range(args.rwin * 2):
                # crop size 1.25x plot scale, adjusted +/-75%; center
                # offset +/-70% of scale on each axis
                ws = max(sc * 1.25 * (1.0 + (np.random.rand() - 0.5) * 1.5), 0.15)
                cx = mx + (np.random.rand() - 0.5) * 1.4 * sc
                cy = my + (np.random.rand() - 0.5) * 1.4 * sc
                ox, oy = cx - ws / 2, cy - ws / 2
                wl = [(lx - ox) / ws, (lyv - oy) / ws,
                      (rx - ox) / ws, (ry - oy) / ws, sc / ws]
                l_in = 0.0 <= wl[0] <= 1.0 and 0.0 <= wl[1] <= 1.0
                r_in = 0.0 <= wl[2] <= 1.0 and 0.0 <= wl[3] <= 1.0
                offl = 0.0 if (l_in or r_in) else 1.0
                xs.append(crop_tensor(gray, cx, cy, ws))
                ys.append(wl + [offl, 0.0])
            # BIG search windows (50-70% of the image): refine learns
            # to find the eyes in a wide view, so the testbed can run
            # refine alone as its own detector
            for _ in range(4):
                ws = 0.5 + np.random.rand() * 0.2
                cx = mx + (np.random.rand() - 0.5) * 0.5 * ws
                cy = my + (np.random.rand() - 0.5) * 0.5 * ws
                ox, oy = cx - ws / 2, cy - ws / 2
                wl = [(lx - ox) / ws, (lyv - oy) / ws,
                      (rx - ox) / ws, (ry - oy) / ws, sc / ws]
                l_in = 0.0 <= wl[0] <= 1.0 and 0.0 <= wl[1] <= 1.0
                r_in = 0.0 <= wl[2] <= 1.0 and 0.0 <= wl[3] <= 1.0
                offl = 0.0 if (l_in or r_in) else 1.0
                xs.append(crop_tensor(gray, cx, cy, ws))
                ys.append(wl + [offl, 0.0])
    # 6th column: off_screen; 7th: clip-window marker (eval split
    # only). shifted/clip variants may carry their own off label
    ys = [lab + [0.0, 0.0] if len(lab) == 5 else lab + [1.0] for lab in ys]
    if args.model == 'target':
        nd = noise_dir(f'/src/hyperspace-sessions/{args.session.split(",")[0]}')
        neg = 0
        i = 0
        while os.path.exists(os.path.join(nd, f'{i}.agi')):
            png = os.path.join(nd, f'{i}-top.png')
            i += 1
            if not os.path.exists(png):
                continue
            gray = np.asarray(Image.open(png).convert('L'))
            xs.append(crop_tensor(gray, 0.0, 0.0, 0.0))
            ys.append([0.5, 0.5, 0.5, 0.5, 0.2, 1.0, 0.0])
            neg += 1
        if neg:
            print(f'noise: {neg} off-screen negatives')
    x = np.array(xs, np.float32)
    y = np.array(ys, np.float32)
    return x, y


class PlotNet(nn.Module):
    # eye plotter: no-pool soft-argmax coordinates + pooled appearance
    def __init__(self):
        super().__init__()
        k = 8
        self.trunk = nn.Sequential(
            nn.Conv2d(3, 32, 3, padding=1), nn.ReLU(),
            nn.Conv2d(32, 32, 3, padding=1), nn.ReLU())
        self.heat = nn.Conv2d(32, k, 1)
        self.register_buffer('cgrid', coord_grid(args.size))
        s = args.size // 4
        self.app = nn.Sequential(
            nn.MaxPool2d(2),
            nn.Conv2d(32, 32, 3, padding=1), nn.ReLU(),
            nn.MaxPool2d(2), nn.Flatten(),
            nn.Linear(s * s * 32, 64), nn.ReLU())
        self.head = nn.Sequential(nn.Linear(3 * k + 64, 64), nn.ReLU(),
                                  nn.Linear(64, 6))   # 5 coords + off_screen logit
        self.register_buffer('lin', torch.linspace(0.0, 1.0, args.size))

    def forward(self, x):
        g = self.cgrid.unsqueeze(0).expand(x.shape[0], -1, -1, -1)
        t = self.trunk(torch.cat([x, g], 1))
        hm = self.heat(t)
        B, k, S, _ = hm.shape
        p = torch.softmax(hm.reshape(B, k, -1) * 8.0, -1).reshape(B, k, S, S)
        xs = (p.sum(2) * self.lin).sum(-1)
        ys = (p.sum(3) * self.lin).sum(-1)
        pk = hm.reshape(B, k, -1).max(-1).values * 0.1
        return self.head(torch.cat([xs, ys, pk, self.app(t)], 1))


def agi_strip(path, keys):
    s = open(path).read()
    for key in keys:
        s = re.sub(rf'^\s*{re.escape(key)}:.*\n?', '', s, flags=re.M)
    open(path, 'w').write(s)


def agi_put(path, key, val):
    s = open(path).read() if os.path.exists(path) else ''
    line = f'{key}: {val}\n'
    if re.search(rf'^{re.escape(key)}:', s, re.M):
        s = re.sub(rf'^{re.escape(key)}:.*\n?', line, s, count=1, flags=re.M)
    else:
        s += line
    open(path, 'w').write(s)


def plot_preview(gray, lx, lyv, rx, ry, sc, out_path):
    # full frame on a black margin, crop windows drawn UNSHRUNK
    from PIL import Image
    h, w = gray.shape
    pad = w // 3
    fr = np.zeros((h + 2 * pad, w + 2 * pad), np.uint8)
    fr[pad:pad + h, pad:pad + w] = gray
    def rect(cx, cy, side):
        x0 = int(np.clip((cx - side / 2) * w + pad, 0, fr.shape[1] - 1))
        x1 = int(np.clip((cx + side / 2) * w + pad, 0, fr.shape[1] - 1))
        y0 = int(np.clip((cy - side / 2) * w + pad, 0, fr.shape[0] - 1))
        y1 = int(np.clip((cy + side / 2) * w + pad, 0, fr.shape[0] - 1))
        fr[y0:y1 + 1, x0] = fr[y0:y1 + 1, x1] = 255
        fr[y0, x0:x1 + 1] = fr[y1, x0:x1 + 1] = 255
    es = max(sc / args.eye_div, 0.03)
    rect(lx, lyv, es)
    rect(rx, ry, es)
    rect((lx + rx) / 2, (lyv + ry) / 2, max(sc * 2.0, 0.1))
    Image.fromarray(fr).save(out_path)


def annotate_frames():
    # torch target+refine plot the eyes on every unannotated frame
    from PIL import Image
    here = os.path.dirname(os.path.abspath(__file__))
    dev = 'cuda' if torch.cuda.is_available() else 'cpu'
    nets = {}
    gp = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models', 'noise.pt')
    gate = None
    if os.path.exists(gp):
        gate = NoiseNet().to('cuda' if torch.cuda.is_available() else 'cpu')
        gate.load_state_dict(torch.load(gp))
        gate.eval()
    for nm in ('target', 'refine'):
        m = PlotNet().to(dev)
        m.load_state_dict(torch.load(os.path.join(here, 'models', f'{nm}.pt'),
                                     map_location=dev))
        m.eval()
        nets[nm] = m
    # face-focus estimation for volume_look needs an initial look model
    look_net = None
    lp = os.path.join(here, 'models', 'look.pt')
    if os.path.exists(lp):
        look_net = CenterNet().to(dev)
        look_net.load_state_dict(torch.load(lp, map_location=dev))
        look_net.eval()
    sd = f'/src/hyperspace-sessions/{args.session}'
    dirs = [sd] + [os.path.join(sd, s) for s in ('volume_look', 'volume_head')
                   if os.path.isdir(os.path.join(sd, s))]
    done = faced = gated = 0

    with torch.no_grad():
        for d in dirs:
            is_vol = os.path.basename(d).startswith('volume_')
            # raw volume dirs are never mutated: results land in
            # applied_<kind>/ so the op stays a repeatable process
            out_d = d
            if is_vol:
                out_d = os.path.join(sd, 'applied_' + d.split('volume_')[-1])
                os.makedirs(out_d, exist_ok=True)
                os.makedirs(os.path.join(out_d, 'preview'), exist_ok=True)
            is_vlook = d.endswith('volume_look')
            i = 0
            while os.path.exists(os.path.join(d, f'{i}.agi')):
                ap = os.path.join(out_d, f'{i}.agi')
                png = os.path.join(d, f'{i}-top.png')
                if is_vol and not os.path.exists(ap):
                    open(ap, 'w').write(open(os.path.join(d, f'{i}.agi')).read())
                i += 1
                text = open(ap).read()
                if not os.path.exists(png):
                    continue
                lx, lyv = read_pair(text, 'top_left')
                # volume frames are ALWAYS re-plotted: applied is a pure
                # function of raw + the current models. gold main-dir
                # annotations are never touched.
                need_eyes = is_vol or lx <= -900
                # BOTH volume kinds get face-focus: even head-tracking
                # doesn't put the face exactly on the dot — center
                # becomes the estimate, center+offset = dot stays exact
                need_face = is_vol and look_net is not None
                if not need_eyes and not need_face:
                    continue
                gray = np.asarray(Image.open(png).convert('L'))
                if need_eyes:
                    full = crop_tensor(gray, 0.0, 0.0, 0.0)
                    if gate is not None:
                        # volume frames drop on a HINT of off-camera:
                        # P(off) > 0.35 excludes; main dir at 0.5
                        p_off = 1.0 - float(torch.sigmoid(
                            gate(nchw(full[None]).to(dev))[0, 0]))
                        thr = 0.35 if is_vol else 0.5
                        if p_off > thr:
                            # stale plots from earlier runs must not
                            # survive a gate rejection
                            agi_strip(ap, ('top_left', 'top_right',
                                           'top_scale', '    head'))
                            agi_put(ap, 'off_screen', 'true')
                            gated += 1
                            continue
                    t5 = nets['target'](nchw(full[None]).to(dev))[0].cpu().numpy()
                    if len(t5) > 5:
                        p_off2 = 1.0 / (1.0 + np.exp(-float(t5[5])))
                        if p_off2 > (0.35 if is_vol else 0.5):
                            agi_strip(ap, ('top_left', 'top_right',
                                           'top_scale', '    head'))
                            agi_put(ap, 'off_screen', 'true')
                            gated += 1
                            continue
                    lx, lyv, rx, ry, sc = [float(v) for v in t5[:5]]
                    fcx0 = (lx + rx) / 2
                    fcy0 = (lyv + ry) / 2
                    # margin: PARTIALLY clipped faces carry accurate
                    # plots and are refine's most valuable training data
                    if not (-0.15 <= fcx0 <= 1.15 and -0.15 <= fcy0 <= 1.15):
                        continue   # face truly gone: unplotted
                    ws = max(sc * 1.25, 0.15)
                    cx = (lx + rx) / 2
                    cy = (lyv + ry) / 2
                    rwin = crop_tensor(gray, cx, cy, ws)
                    r5 = nets['refine'](nchw(rwin[None]).to(dev))[0].cpu().numpy()
                    if len(r5) > 5:
                        p_off3 = 1.0 / (1.0 + np.exp(-float(r5[5])))
                        if p_off3 > (0.35 if is_vol else 0.5):
                            agi_strip(ap, ('top_left', 'top_right',
                                           'top_scale', '    head'))
                            agi_put(ap, 'off_screen', 'true')
                            gated += 1
                            continue
                    ox, oy = cx - ws / 2, cy - ws / 2
                    # exact set: target -> zoom 125% -> refine
                    lx, lyv = ox + float(r5[0]) * ws, oy + float(r5[1]) * ws
                    rx, ry  = ox + float(r5[2]) * ws, oy + float(r5[3]) * ws
                    sc = float(r5[4]) * ws
                    agi_strip(ap, ('off_screen',))
                    agi_put(ap, 'top_left',  f'{lx} {lyv}')
                    agi_put(ap, 'top_right', f'{rx} {ry}')
                    agi_put(ap, 'top_scale', f'{sc}')
                    if is_vol:
                        plot_preview(gray, lx, lyv, rx, ry, sc,
                                     os.path.join(out_d, 'preview', f'{i - 1}.png'))
                    done += 1
                    if done % 500 == 0:
                        print(f'annotate: {done}')
                el_r = read_pair(text, 'top_right')
                el_s = read_pair(text, 'top_scale')
                if not need_eyes:
                    rx, ry, sc = el_r[0], el_r[1], el_s[0]
                if need_face:
                    # look net estimates where the FACE points; the dot
                    # comes from the RAW file's look (legacy: center)
                    raw = open(os.path.join(d, f'{i - 1}.agi')).read()
                    dot = read_pair(raw, 'look')
                    if dot[0] <= -0.9:
                        dot = read_pair(raw, 'center')
                    side = max(sc / args.eye_div, 0.03)
                    m2 = ((lx + rx) / 2, (lyv + ry) / 2)
                    li = crop_rect(gray, lx, lyv, side * 1.5, side)
                    ri = crop_rect(gray, rx, ry, side * 1.5, side)
                    fi = crop_rect(gray, m2[0], m2[1],
                                   max(sc * 1.8, 0.1), max(sc * 1.2, 0.067))
                    oc9l, oc9r = oc_or_extrap(open(ap).read(), (lx, lyv), (rx, ry))
                    av = np.array([[m2[0], m2[1], sc, lx, lyv, rx, ry,
                                    oc9l[0], oc9l[1], oc9r[0], oc9r[1],
                                    args.oc_span_m]], np.float32)
                    c, g = look_net(nchw(li[None]).to(dev), nchw(ri[None]).to(dev),
                                    nchw(fi[None]).to(dev), torch.from_numpy(av).to(dev))
                    fcx, fcy = float(c[0, 0]), float(c[0, 1])
                    agi_put(ap, '    head', f'{fcx} {fcy}')
                    agi_put(ap, '    look', f'{dot[0]} {dot[1]}')
                    faced += 1
    print(f'annotate: {done} frames eye-plotted, {faced} face-focus estimated, {gated} gate-excluded')


class NoiseNet(nn.Module):
    # face-usable vs off-camera noise: binary on the full frame
    def __init__(self):
        super().__init__()
        s = args.size // 4
        self.net = nn.Sequential(
            nn.Conv2d(1, 16, 3, padding=1), nn.ReLU(),
            nn.MaxPool2d(2),
            nn.Conv2d(16, 32, 3, padding=1), nn.ReLU(),
            nn.MaxPool2d(2), nn.Flatten(),
            nn.Linear(s * s * 32, 64), nn.ReLU(),
            nn.Linear(64, 1))

    def forward(self, x):
        return self.net(x)


def train_noise():
    from PIL import Image
    sd = f'/src/hyperspace-sessions/{args.session}'
    cache = os.path.join(sd, f'.torchcache-noise-{args.size}-v2.npz')
    dirs_pos = [sd] + [os.path.join(sd, s) for s in ('volume_look', 'volume_head')
                       if os.path.isdir(os.path.join(sd, s))]
    nd = noise_dir(sd)
    all_dirs = dirs_pos + ([nd] if os.path.isdir(nd) else [])
    newest = max((os.path.getmtime(os.path.join(d, f))
                  for d in all_dirs for f in os.listdir(d) if f.endswith('.agi')), default=0)
    if os.path.exists(cache) and os.path.getmtime(cache) >= newest:
        z = np.load(cache)
        x, y = z['x'], z['y']
    else:
        print('building noise cache ...')
        xs, ys = [], []
        for d in all_dirs:
            lab = 0.0 if d == nd else 1.0
            for i in agi_ids(d):
                png = os.path.join(d, f'{i}-top.png')
                if not os.path.exists(png):
                    continue
                gray = np.asarray(Image.open(png).convert('L'))
                xs.append(crop_tensor(gray, 0.0, 0.0, 0.0))
                ys.append(lab)
        x = np.array(xs, np.float32)
        y = np.array(ys, np.float32)
        np.savez_compressed(cache, x=x, y=y)
    n = len(y)
    n_neg = int((y < 0.5).sum())
    print(f'noise: {n - n_neg} domain / {n_neg} off-camera')
    order = np.random.permutation(n)
    n_ev = max(1, n // 10)
    ev, tr = order[:n_ev], order[n_ev:]
    dev = 'cuda' if torch.cuda.is_available() else 'cpu'
    m = NoiseNet().to(dev)
    opt = torch.optim.AdamW(m.parameters(), lr=args.lr, weight_decay=args.wd)
    xtr, ytr = nchw(x[tr]).to(dev), torch.from_numpy(y[tr]).to(dev)
    xev, yev = nchw(x[ev]).to(dev), torch.from_numpy(y[ev]).to(dev)
    # class-balanced epoch sampling
    w = np.where(y[tr] > 0.5, 1.0 / max(1, len(tr) - int((y[tr] < 0.5).sum())),
                 1.0 / max(1, int((y[tr] < 0.5).sum())))
    bw = torch.from_numpy((w / w.sum()).astype(np.float32)).to(dev)
    best, best_state = None, None
    for epoch in range(args.epochs):
        m.train()
        order2 = torch.multinomial(bw, len(ytr), replacement=True)
        for b in range(0, len(ytr), args.batch):
            idx = order2[b:b + args.batch]
            xb = xtr[idx]
            # face-or-not survives mirroring: free 2x augmentation
            fm = torch.rand(len(idx), device=xb.device) < 0.5
            if bool(fm.any()):
                xb = xb.clone()
                xb[fm] = torch.flip(xb[fm], [3])
            p = m(xb)[:, 0]
            loss = nn.functional.binary_cross_entropy_with_logits(p, ytr[idx])
            opt.zero_grad()
            loss.backward()
            opt.step()
        m.eval()
        with torch.no_grad():
            p = m(xev)[:, 0]
            acc = float(((p > 0) == (yev > 0.5)).float().mean())
        print(f'epoch {epoch + 1}/{args.epochs}  eval acc {acc:.4f}')
        if best is None or acc > best:
            best = acc
            best_state = {k2: v.clone() for k2, v in m.state_dict().items()}
    if best_state is not None:
        m.load_state_dict(best_state)
        out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models')
        os.makedirs(out, exist_ok=True)
        torch.save(best_state, os.path.join(out, 'noise.pt'))
        print(f'best eval acc {best:.4f} -> {out}/noise.pt')
    print('training complete')


def train_plot():
    sd = f'/src/hyperspace-sessions/{args.session}'
    x, y = build_plot_cache(sd)
    n = len(y)
    order = np.random.permutation(n)
    n_ev = max(1, n // 10)
    ev, tr = order[:n_ev], order[n_ev:]
    print(f'{args.model}: {len(tr)} train / {len(ev)} eval')
    dev = 'cuda' if torch.cuda.is_available() else 'cpu'
    m = PlotNet().to(dev)
    opt = (torch.optim.AdamW(m.parameters(), lr=args.lr, weight_decay=args.wd)
           if args.optimizer == 'adam'
           else torch.optim.SGD(m.parameters(), lr=args.lr, weight_decay=args.wd))
    # organic patch blobs go on TRAIN windows only — eval stays clean.
    # refine gets a light touch: fewer windows, smaller blobs, and a
    # protected disc over the face between the eyes
    xtr_np = x[tr]
    srcs = _patch_sources(sd)
    if len(srcs):
        prob = 0.15 if args.model == 'refine' else 0.5
        thr = 0.72 if args.model == 'refine' else 0.62
        mg = 0.3 if args.model == 'refine' else 0.0
        xtr_np = xtr_np.copy()
        n_p = 0
        for i5 in range(len(xtr_np)):
            if np.random.rand() < prob:
                xtr_np[i5] = _patch_blobs(xtr_np[i5],
                                          srcs[np.random.randint(len(srcs))],
                                          y[tr][i5][:4], thr, mg)
                n_p += 1
        print(f'patched {n_p} train windows from {len(srcs)} organic sources')
    # preview up to 100 of the ACTUAL training windows with labels
    from PIL import Image as PImage, ImageDraw
    pv = os.path.join(sd, 'previews', args.model)
    os.makedirs(pv, exist_ok=True)
    for f5 in os.listdir(pv):
        os.remove(os.path.join(pv, f5))
    ytr_np = y[tr]
    picks = np.random.permutation(len(ytr_np))[:min(100, len(ytr_np) // 100)]
    for i in picks:
        im5 = PImage.fromarray((xtr_np[i][:, :, 0] * 255).astype(np.uint8))
        im5 = im5.resize((160, 160), PImage.NEAREST).convert('RGB')
        d5 = ImageDraw.Draw(im5)
        for k5, col in ((0, (255, 60, 60)), (2, (60, 255, 60))):
            px5, py5 = float(ytr_np[i, k5]) * 160, float(ytr_np[i, k5 + 1]) * 160
            if -10 <= px5 <= 170 and -10 <= py5 <= 170:
                d5.ellipse([px5 - 5, py5 - 5, px5 + 5, py5 + 5],
                           outline=col, width=2)
        im5.save(os.path.join(pv, f'{i}_s{ytr_np[i, 4]:.2f}_off{int(ytr_np[i, 5])}.png'))
    print(f'previews: {len(picks)} -> {pv}')
    xtr = nchw(xtr_np).to(dev)
    ytr = torch.from_numpy(y[tr]).to(dev)
    xev = nchw(x[ev]).to(dev)
    yev = torch.from_numpy(y[ev]).to(dev)
    cols = ['left.x', 'left.y', 'right.x', 'right.y', 'scale', 'off.err', 'clip.err']
    print(f'==================== {args.model.upper()} ====================')
    print(pad('epoch', 10) + pad('train') + pad('eval') +
          ''.join(pad(c) for c in cols))
    best, best_state, best_ep = None, None, 0
    bs = args.batch
    for epoch in range(args.epochs):
        m.train()
        o2 = torch.randperm(len(ytr), device=dev)
        tot = 0.0
        for b in range(0, len(ytr), bs):
            idx = o2[b:b + bs]
            xb = xtr[idx]
            if args.noise > 0:
                xb = xb + torch.randn_like(xb) * args.noise
            p = m(xb)
            pm = (1.0 - ytr[idx, 5:6])
            if args.model == 'refine':
                # clipped windows keep accurate coords: supervise them
                pm = torch.ones_like(pm)
            coord = (((p[:, :5] - ytr[idx, :5]) ** 2) * pm).sum() / (pm.sum() * 5 + 1e-6)
            offb  = nn.functional.binary_cross_entropy_with_logits(p[:, 5], ytr[idx, 5:6].squeeze(1))
            loss = coord + 0.2 * offb
            opt.zero_grad()
            loss.backward()
            opt.step()
            tot += float(coord.detach()) * len(idx)
        m.eval()
        with torch.no_grad():
            p = m(xev)
            # NORMAL windows score the main columns; clip windows get
            # their own aggregate so the table stays comparable
            nmm = yev[:, 6] < 0.5
            onm = nmm & (yev[:, 5] < 0.5)
            eval_loss = float(((p[onm, :5] - yev[onm, :5]) ** 2).mean())
            errs = (p[onm, :5] - yev[onm, :5]).abs().mean(0).cpu().numpy()
            oacc = float(((p[:, 5] > 0) == (yev[:, 5] > 0.5)).float().mean())
            clipm = ~nmm
            cerr = (float((p[clipm, :4] - yev[clipm, :4]).abs().mean())
                    if bool(clipm.any()) else 0.0)
            errs = np.concatenate([errs, [1.0 - oacc], [cerr]])
        print(pad(f'{epoch + 1}/{args.epochs}', 10) +
              pad(f'{tot / len(ytr):.6g}') + pad(f'{eval_loss:.6g}') +
              ''.join(pad(f'{e:.6g}') for e in errs))
        mval = float(errs.mean())
        if best is None or mval < best:
            best, best_ep = mval, epoch + 1
            best_state = {k2: v.clone() for k2, v in m.state_dict().items()}
    if best_state is not None:
        out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models')
        os.makedirs(out, exist_ok=True)
        torch.save(best_state, os.path.join(out, f'{args.model}.pt'))
        print(f'best epoch {best_ep} (avg err {best:.6g}) -> {out}/{args.model}.pt')
    print('training complete')


def export_ts():
    # trace trained models for the app's libtorch runtime
    here = os.path.dirname(os.path.abspath(__file__))
    out = '/src/silver/platform/native/share/hyperspace/models'
    os.makedirs(out, exist_ok=True)

    class LookWrap(nn.Module):
        # applies the anti-compression calibration measured at training
        def __init__(self, m, cal):
            super().__init__()
            self.m = m
            self.register_buffer('slope', torch.from_numpy(cal[:, 0]))
            self.register_buffer('bias',  torch.from_numpy(cal[:, 1]))

        def forward(self, l, r, f, a):
            c, g = self.m(l, r, f, a)
            return torch.cat([c, g], 1) * self.slope + self.bias

    S = args.size
    jobs = [('target.pt', PlotNet, 'target.ptc'),
            ('refine.pt', PlotNet, 'refine.ptc'),
            ('noise.pt', NoiseNet, 'noise.ptc'),
            ('look.pt', CenterNet, 'look.ptc')]
    for src, cls, dst in jobs:
        p = os.path.join(here, 'models', src)
        if not os.path.exists(p):
            print(f'export_ts: {src} missing, skipped')
            continue
        m = cls()
        m.load_state_dict(torch.load(p, map_location='cpu'))
        m.eval()
        if cls is CenterNet:
            calp = p.replace('.pt', '_cal.npy')
            cal = (np.load(calp) if os.path.exists(calp)
                   else np.tile(np.array([[1.0, 0.0]], np.float32), (4, 1)))
            m = LookWrap(m, cal.astype(np.float32))
            ex = (torch.zeros(1, 1, S, S), torch.zeros(1, 1, S, S),
                  torch.zeros(1, 1, S, S), torch.zeros(1, 12))
        else:
            ex = (torch.zeros(1, 1, S, S),)
        ts = torch.jit.trace(m, ex)
        ts.save(os.path.join(out, dst))
        print(f'exported {dst} -> {out}')


def run_base(with_look):
    # base set: target, refine, gate (when noise/ exists), and the
    # gold-only look that face-focus estimation depends on.
    # every stage runs the STATED epoch count except the gold look (2x)
    base_e = args.epochs
    for nm in ('target', 'refine'):
        args.model = nm
        args.epochs = base_e
        print(f'== base: training {nm} ({args.epochs} epochs) ==')
        train_plot()
    args.epochs = base_e
    if os.path.isdir(noise_dir(f'/src/hyperspace-sessions/{args.session}')):
        print('== base: training noise (domain vs off-camera) ==')
        train_noise()
    if with_look:
        args.model = 'look'
        args.volume = 0
        print(f'== base: training look ({args.epochs} epochs) ==')
        train_look()
    args.epochs = base_e
    export_ts()


def data_stats():
    from PIL import Image as PImage
    sd = f'/src/hyperspace-sessions/{args.session}'
    limgs, laux, ly, lg, lfid = load_session(sd)
    n = len(ly)
    src_names = np.where(lfid < 100000, 'gold',
                np.where(lfid < 200000, 'vlook', 'vhead'))
    print(f'== dataset variety: {args.session} ({n} look samples) ==')
    for s in ('gold', 'vlook', 'vhead'):
        print(f'  {s}: {int((src_names == s).sum())}')
    def grid_report(name, pts, gx, gy, lo, hi):
        b_x = np.clip(((pts[:, 0] - lo) / (hi - lo) * gx).astype(int), 0, gx - 1)
        b_y = np.clip(((pts[:, 1] - lo) / (hi - lo) * gy).astype(int), 0, gy - 1)
        cnt = np.bincount(b_y * gx + b_x, minlength=gx * gy).astype(float)
        occ = (cnt > 0).sum()
        p = cnt / cnt.sum()
        ent = -np.nansum(np.where(p > 0, p * np.log(p), 0)) / np.log(gx * gy)
        print(f'  {name}: {int(occ)}/{gx * gy} bins occupied, '
              f'uniformity {ent:.2f} (1 = perfectly even), '
              f'min {int(cnt[cnt > 0].min()) if occ else 0} max {int(cnt.max())}')
        img = (cnt.reshape(gy, gx) / max(cnt.max(), 1) * 255).astype(np.uint8)
        os.makedirs(os.path.join(sd, 'stats'), exist_ok=True)
        PImage.fromarray(np.kron(img, np.ones((24, 24), np.uint8))).save(
            os.path.join(sd, 'stats', f'{name}.png'))
    grid_report('gaze-coverage', ly[:, 2:4], 16, 9, -0.5, 0.5)
    grid_report('head-direction', ly[:, :2], 16, 9, -0.5, 0.5)
    grid_report('head-in-frame', laux[:, :2], 12, 12, 0.0, 1.0)
    sc = laux[:, 2]
    print(f'  depth (scale): mean {sc.mean():.3f}  span {sc.min():.3f}..{sc.max():.3f}  '
          f'std {sc.std():.3f}')
    br = np.array([limgs[2][i].mean() for i in range(n)])
    print(f'  face brightness: mean {br.mean():.3f}  std {br.std():.3f}  '
          f'span {br.min():.3f}..{br.max():.3f}')
    # redundancy: fraction of samples whose nearest neighbor in
    # (head-in-frame, scale, gaze) space is nearly identical
    F = np.concatenate([laux[:, :3], ly[:, 2:4]], 1)
    k = min(n, 4000)
    idx = np.random.permutation(n)[:k]
    Fs = F[idx]
    D = np.linalg.norm(Fs[:, None, :] - Fs[None, :, :], axis=2)
    np.fill_diagonal(D, np.inf)
    nn = D.min(1)
    print(f'  redundancy: {100 * (nn < 0.01).mean():.1f}% of samples have a '
          f'near-twin (<0.01 in pose+gaze space)')
    print(f'  coverage maps -> {sd}/stats/')
def main():
    assert args.cams == 1, 'torch trainer is single-cam'
    if args.seed:
        np.random.seed(args.seed)
        torch.manual_seed(args.seed)
    if args.stats:
        data_stats()
        return
    if args.export_ts:
        export_ts()
        return
    if args.process == 'base':
        run_base(with_look=True)
        return
    if args.process == 'gen':
        # one full generation: fresh applied dirs, base from gold,
        # apply volumes, base AGAIN on the smoothed volume labels,
        # re-apply volumes with the sharper plotters, final look.
        # look gets the double improvement on volume.
        import shutil, glob
        sd2 = f'/src/hyperspace-sessions/{args.session}'
        for sub in ('applied_look', 'applied_head'):
            p2 = os.path.join(sd2, sub)
            if os.path.isdir(p2):
                shutil.rmtree(p2)
        for c2 in glob.glob(os.path.join(sd2, '.torchcache-*')):
            os.remove(c2)
        print('== gen: applied dirs + caches cleared ==')
        print('== gen 1/5: base (gold) ==')
        run_base(with_look=True)
        has_vol = any(os.path.isdir(os.path.join(sd2, f'volume_{s}'))
                      for s in ('look', 'head'))
        if not has_vol:
            # no volume recordings: a second base would retrain on
            # the exact same counts — base IS the whole generation
            print('== gen: no volumes recorded, done at base ==')
            export_ts()
            return
        print('== gen 2/5: volumes (first application) ==')
        annotate_frames()
        print('== gen 3/5: base again (volume-smoothed labels) ==')
        run_base(with_look=False)
        print('== gen 4/5: volumes (re-application) ==')
        annotate_frames()
        args.epochs = args.epochs * 2
        print(f'== gen 5/5: final look ({args.epochs} epochs) ==')
        args.model = 'look'
        args.volume = 1
        train_look()
        export_ts()
        return
    if args.process == 'volumes':
        # process 2: apply ops over both volumes — eye plots + face
        # focus (base models required)
        annotate_frames()
        return
    if args.process == 'look':
        train_look()
        export_ts()
        return
    if args.process == 'noise':
        train_noise()
        export_ts()
        return
    if args.process in ('target', 'refine'):
        args.model = args.process
        train_plot()
        export_ts()
        return
    if args.annotate:
        annotate_frames()
        return
    train_look()


def train_look():
    sd = f'/src/hyperspace-sessions/{args.session}'
    limgs, laux, ly, lg, lfid = load_session(sd)
    if args.group_by == 'pose':
        # face-look grouping: same head pose (med+scale bins) is ONE
        # unit — eval never shares a pose with training
        keys = {}
        lg = np.array([keys.setdefault(
            (round(a[0] / 0.06), round(a[1] / 0.06), round(a[2] / 0.03)),
            len(keys)) for a in laux])
    # hold out whole groups; no augmentation anywhere
    if args.group_by == 'frame':
        # sparse islands: every sequence feeds BOTH sides — hold out
        # a deterministic 10% of frames within each group
        import hashlib
        evm = np.array([int(hashlib.md5(f'{args.seed}_{f}'.encode()).hexdigest(), 16) % 10 == 0
                        for f in lfid])
        ev = np.where(evm)[0]
        tr = np.where(~evm)[0]
    elif args.group_by == 'headdir':
        # hash of binned head direction -> deterministic 10% eval;
        # every frame sharing a head-look lands on the same side
        import hashlib
        def hd(y):
            key = f'{round(y[0] / 0.05)}_{round(y[1] / 0.05)}'
            return int(hashlib.md5(key.encode()).hexdigest(), 16)
        evm = np.array([hd(y) % 10 == 0 for y in ly])
        ev = np.where(evm)[0]
        tr = np.where(~evm)[0]
    else:
        gids = np.unique(lg)
        np.random.shuffle(gids)
        ev_g = set(gids[:max(1, len(gids) // 10)])
        ev = np.array([i for i in range(len(ly)) if lg[i] in ev_g])
        tr = np.array([i for i in range(len(ly)) if lg[i] not in ev_g])
    if args.fraction < 1.0:
        keep = np.random.permutation(len(tr))[:int(len(tr) * args.fraction)]
        tr = tr[np.sort(keep)]
    if args.zero_aux:
        laux = np.zeros_like(laux)
    if args.preview:
        # every canonical training sample: crops + source + labels in
        # the filename — review what look actually learns from
        from PIL import Image as PImage
        pd = os.path.join(sd, 'preview-look')
        os.makedirs(pd, exist_ok=True)
        def up4(t):
            return np.kron((t[:, :, 0] * 255).astype(np.uint8),
                           np.ones((4, 4), np.uint8))
        for i in tr:
            row = np.hstack([up4(limgs[k][i]) for k in range(3)])
            fid = int(lfid[i])
            src2 = 'gold' if fid < 100000 else ('vlook' if fid < 200000 else 'vhead')
            fn = (f'{src2}_{fid % 100000:05d}'
                  f'_h{ly[i][0]:.3f}-{ly[i][1]:.3f}'
                  f'_g{ly[i][2]:.3f}-{ly[i][3]:.3f}.png')
            PImage.fromarray(row).save(os.path.join(pd, fn))
        print(f'preview: {len(tr)} look samples -> {pd}')
    # fake head translation: SAME crops, aux shifted by d, screen point
    # shifted k*d/scale (closer head = bigger scale = smaller shift)
    timgs = [limgs[k][tr] for k in range(3)]
    taux, tly = laux[tr], ly[tr]
    # fake depth: crops are scale-proportional so pixels are UNCHANGED;
    # frame positions spread by f from camera center, scale *= f, and
    # both screen points move along the eye ray: P' = E + (P-E)/f
    if args.saug:
        c0 = np.arange(len(tr))
        bi = np.repeat(c0, args.saug)
        f = np.exp(np.random.uniform(-np.log(args.sspan), np.log(args.sspan), len(bi))).astype(np.float32)
        base_a = laux[tr][bi]
        base_y = ly[tr][bi]
        a2 = base_a.copy()
        for col in (0, 1, 3, 4, 5, 6, 7, 8, 9, 10):
            a2[:, col] = 0.5 + (a2[:, col] - 0.5) * f
        a2[:, 2] = base_a[:, 2] * f
        s0 = base_a[:, 2]
        Ex = 0.0 + args.ekx * (base_a[:, 0] - 0.5) / s0
        Ey = args.ey0 + args.eky * (base_a[:, 1] - 0.5) / s0
        inv = 1.0 / f
        y2 = base_y.copy()
        y2[:, 0] = Ex + (y2[:, 0] - Ex) * inv
        y2[:, 2] = Ex + (y2[:, 2] - Ex) * inv
        y2[:, 1] = Ey + (y2[:, 1] - Ey) * inv
        y2[:, 3] = Ey + (y2[:, 3] - Ey) * inv
        timgs = [np.concatenate([timgs[k], timgs[k][bi]]) for k in range(3)]
        taux  = np.concatenate([taux, a2]).astype(np.float32)
        tly   = np.concatenate([tly, y2]).astype(np.float32)
    if args.taug:
        bi = np.repeat(np.arange(len(tr)), args.taug)
        dx = (np.random.rand(len(bi)) - 0.5) * 2.0 * args.trange
        dy = (np.random.rand(len(bi)) - 0.5) * 2.0 * args.trange
        a2 = taux[bi].copy()
        for col in (0, 3, 5, 7, 9): a2[:, col] += dx
        for col in (1, 4, 6, 8, 10): a2[:, col] += dy
        sc = taux[bi][:, 2]
        y2 = tly[bi].copy()
        y2[:, 0] += args.tkx * dx / sc
        y2[:, 2] += args.tkx * dx / sc
        y2[:, 1] += args.tky * dy / sc
        y2[:, 3] += args.tky * dy / sc
        timgs = [np.concatenate([timgs[k], timgs[k][bi]]) for k in range(3)]
        taux  = np.concatenate([taux, a2]).astype(np.float32)
        tly   = np.concatenate([tly, y2]).astype(np.float32)
    n_aug = max(len(tly), 100000)
    print(f'look: {len(tly)} source frames -> {n_aug} augmented per epoch'
          f' / {len(ev)} eval ({args.group_by} holdout)')

    dev = 'cuda' if torch.cuda.is_available() else 'cpu'
    m = CenterNet().to(dev)
    opt = (torch.optim.AdamW(m.parameters(), lr=args.lr, weight_decay=args.wd)
           if args.optimizer == 'adam'
           else torch.optim.SGD(m.parameters(), lr=args.lr, weight_decay=args.wd))

    xtr = [nchw(timgs[k]).to(dev) for k in range(3)]
    atr = torch.from_numpy(taux).to(dev)
    ytr = torch.from_numpy(tly).to(dev)
    gtr = (torch.from_numpy(lg[tr].astype(np.int64)).to(dev)
           if len(lg[tr]) == len(tly) else None)
    xev = [nchw(limgs[k][ev]).to(dev) for k in range(3)]
    aev = torch.from_numpy(laux[ev]).to(dev)
    yev = torch.from_numpy(ly[ev]).to(dev)

    n = len(ytr)
    bs = args.batch
    # gaze-bin balancing: per-epoch sampling weighted 1/bin_count so
    # every screen region contributes equally (kills center bias)
    bw = None
    if args.balance:
        # 16x9 screen areas: a bin with 1/10th the count of the
        # densest gets sampled 10x as often (weights are 1/count)
        g5 = tly[:, 2:4] + 0.5
        bx = np.clip((g5[:, 0] * args.bins_x).astype(int), 0, args.bins_x - 1)
        by = np.clip((g5[:, 1] * args.bins_y).astype(int), 0, args.bins_y - 1)
        bid = by * args.bins_x + bx
        cnt = np.bincount(bid, minlength=args.bins_x * args.bins_y).astype(np.float64)
        # count normalization: every occupied bin draws an equal
        # share — then CENTER bins (middle third of the screen) get
        # 4x weight, since the center is where it performs worst
        w5 = 1.0 / cnt[bid]
        ctr = ((np.abs(tly[:, 2]) < 0.17) & (np.abs(tly[:, 3]) < 0.17))
        w5[ctr] *= 4.0
        bw = torch.from_numpy((w5 / w5.sum()).astype(np.float32)).to(dev)
        occ = cnt[cnt > 0]
        print(f'balance: {len(occ)} occupied bins, counts {int(occ.min())}..{int(occ.max())}')
    cols = ['head.x', 'head.y', 'gaze.x', 'gaze.y']
    print('==================== LOOK ====================')
    print(pad('epoch', 10) + pad('train') + pad('eval') +
          ''.join(pad(c) for c in cols))
    best, best_state, best_ep = None, None, 0
    # every epoch is 100k samples: each draw repeats a frame with a
    # fresh random translation, so the islands fill into a continuum
    draw = max(n, 100000)
    # staged: first half trains the HEAD expert (face column only),
    # second half freezes it and trains the eye columns + gaze
    head_epochs = args.epochs // 2
    frozen = False
    for epoch in range(args.epochs):
        if epoch >= head_epochs and not frozen:
            frozen = True
            for mod in (m.face, m.colF, m.center):
                for prm in mod.parameters():
                    prm.requires_grad_(False)
            print(f'-- head frozen at epoch {epoch + 1}: training gaze --')
        m.train()
        if bw is not None:
            order = torch.multinomial(bw, draw, replacement=True)
        else:
            order = torch.randint(0, n, (draw,), device=dev)
        tot = 0.0
        for b in range(0, draw, bs):
            idx = order[b:b + bs]
            xb = []
            for x in xtr:
                v = x[idx]
                if args.photo > 0:
                    # per-crop contrast/brightness jitter, on-GPU
                    cc = 1.0 + (torch.rand(v.shape[0], 1, 1, 1, device=v.device) - 0.5) * 2.0 * args.photo
                    bb = (torch.rand(v.shape[0], 1, 1, 1, device=v.device) - 0.5) * 2.0 * args.photo
                    v = ((v - 0.5) * cc + 0.5 + bb).clamp(0.0, 1.0)
                xb.append(v)
            # mirror half the batch: crops flip and swap eyes, screen
            # x labels negate — the aux is unused by the net now
            mir = (torch.rand(len(idx), 1, 1, 1, device=dev) < 0.5) & (args.mirror > 0)
            mv = mir.reshape(-1)
            lf = torch.flip(xb[1], [3])
            rf = torch.flip(xb[0], [3])
            xb[0] = torch.where(mir, lf, xb[0])
            xb[1] = torch.where(mir, rf, xb[1])
            xb[2] = torch.where(mir, torch.flip(xb[2], [3]), xb[2])
            yb = ytr[idx].clone()
            ybm = yb.clone()
            ybm[:, 0] = 0.0 - yb[:, 0]
            ybm[:, 2] = 0.0 - yb[:, 2]
            yb = torch.where(mv[:, None], ybm, yb)
            # aux mirrors with the crops: camera x -> 1-x, eyes swap
            ab = atr[idx]
            abm = ab.clone()
            abm[:, 0] = 1.0 - ab[:, 0]
            abm[:, 3] = 1.0 - ab[:, 5]
            abm[:, 4] = ab[:, 6]
            abm[:, 5] = 1.0 - ab[:, 3]
            abm[:, 6] = ab[:, 4]
            abm[:, 7] = 1.0 - ab[:, 9]
            abm[:, 8] = ab[:, 10]
            abm[:, 9] = 1.0 - ab[:, 7]
            abm[:, 10] = ab[:, 8]
            ab = torch.where(mv[:, None], abm, ab)
            # mixup, LOCAL only: blend with a shuffled partner when
            # both head and gaze labels sit within 0.05 of the screen
            # — interpolation stays physically plausible
            lam = torch.rand(len(idx), 1, device=dev) * 0.5
            pj = torch.randperm(len(idx), device=dev)
            near = ((yb - yb[pj]).abs().max(1).values < 0.05).float().reshape(-1, 1)
            lam = lam * near
            l4 = lam.reshape(-1, 1, 1, 1)
            for k4 in range(3):
                xb[k4] = xb[k4] * (1.0 - l4) + xb[k4][pj] * l4
            ab = ab * (1.0 - lam) + ab[pj] * lam
            yb = yb * (1.0 - lam) + yb[pj] * lam
            c, g = m(*xb, ab)
            # delta supervised directly against the measured eye offset:
            # keeps the eyes' head from absorbing center errors
            d_true = yb[:, 2:4] - yb[:, :2]
            # slope matching: pred-vs-true regression slope must be 1
            # per axis — forbids compression toward the mean without
            # rewarding a blind stretch (which wrecked the middle)
            def slope1(p, y):
                yc = y - y.mean(0)
                pc = p - p.mean(0)
                s = (pc * yc).mean(0) / (yc * yc).mean(0).clamp_min(1e-6)
                return ((1.0 - s) ** 2).mean()
            # same group = same head: the head output may not react
            # to the eyes roaming inside the face crop
            cterm = c.sum() * 0.0
            if gtr is not None:
                sg = (gtr[idx] == gtr[idx][pj]).float()
                cterm = (((c - c[pj]) ** 2).sum(1) * sg).mean()
            # zero cost inside eps, quadratic beyond, and 10x extra
            # on anything off by more than 0.1 of the screen
            def tol(e):
                a2 = e.abs()
                return ((torch.relu(a2 - args.eps) ** 2).mean()
                        + 10.0 * (torch.relu(a2 - 0.1) ** 2).mean())
            if epoch < head_epochs:
                # stage 1: head expert only
                loss = (tol(c - yb[:, :2])
                        + 0.5 * slope1(c, yb[:, :2])
                        + 1.0 * cterm)
            else:
                # stage 2: gaze on top of the frozen head
                loss = (2.0 * tol(g - yb[:, 2:4])
                        + 2.0 * tol((g - c) - d_true)
                        + 0.5 * slope1(g, yb[:, 2:4]))
            opt.zero_grad()
            loss.backward()
            opt.step()
            # report plain mse so train and eval columns are comparable
            plain = float(((torch.cat([c, g], 1) - yb) ** 2).mean().detach())
            tot += plain * len(idx)
        m.eval()
        with torch.no_grad():
            c, g = m(*xev, aev)
            p = torch.cat([c, g], 1)
            eval_loss = float(((p - yev) ** 2).mean())
            errs = (p - yev).abs().mean(0).cpu().numpy()
        print(pad(f'{epoch + 1}/{args.epochs}', 10) +
              pad(f'{tot / draw:.6g}') + pad(f'{eval_loss:.6g}') +
              ''.join(pad(f'{e:.6g}') for e in errs))
        mval = float(errs.mean())
        if best is None or mval < best:
            best, best_ep = mval, epoch + 1
            best_state = {k: v.clone() for k, v in m.state_dict().items()}
    if best_state is not None:
        m.load_state_dict(best_state)
        out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models')
        os.makedirs(out, exist_ok=True)
        # regression compresses toward the mean: measure the per-axis
        # expansion (pred -> true) and export it with the model
        m.eval()
        with torch.no_grad():
            c, g = m(*xtr, atr)
            pr = torch.cat([c, g], 1).cpu().numpy()
        ty2 = tly[:len(pr)]
        cal = np.zeros((4, 2), np.float32)
        for k2 in range(4):
            s2, b2 = np.polyfit(pr[:, k2], ty2[:, k2], 1)
            cal[k2] = (s2, b2)
        print('calibration slopes:', np.round(cal[:, 0], 3))
        torch.save(best_state, os.path.join(out, 'look.pt'))
        np.save(os.path.join(out, 'look_cal.npy'), cal)
        print(f'best epoch {best_ep} (avg err {best:.6g}) -> {out}/look.pt')
    print('training complete')


if __name__ == '__main__':
    main()
