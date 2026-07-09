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
    p.add_argument('--session',   default='basic')
    p.add_argument('--epochs',    type=int,   default=30)
    p.add_argument('--optimizer', default='adam', choices=['adam', 'sgd'])
    p.add_argument('--lr',        type=float, default=0.001)
    p.add_argument('--batch',     type=int,   default=32)
    p.add_argument('--train',     action='store_true')   # parity with silver CLI
    p.add_argument('--size',      type=int,   default=32)
    p.add_argument('--cams',      type=int,   default=1)
    p.add_argument('--seed',      type=int,   default=1234)
    p.add_argument('--zero_aux',  type=int,   default=0)
    p.add_argument('--dropout',   type=float, default=0.2)
    p.add_argument('--wd',        type=float, default=1e-4)   # weight decay (AdamW)
    p.add_argument('--noise',     type=float, default=0.02)   # train pixel noise
    p.add_argument('--aux_noise', type=float, default=0.005)  # train aux jitter
    p.add_argument('--eye_div',   type=float, default=2.12)   # eye crop side = scale / eye_div (2x area vs /3)
    p.add_argument('--taug',   type=int,   default=0)     # fake-translation variants per sample
    p.add_argument('--trange', type=float, default=0.1)   # max |head shift|, frame units
    p.add_argument('--tkx',    type=float, default=1.0)   # screen shift = tk * shift / scale
    p.add_argument('--tky',    type=float, default=1.0)
    p.add_argument('--saug',   type=int,   default=0)     # fake-depth variants per sample
    p.add_argument('--sspan',  type=float, default=1.8)   # scale factor range [1/sspan, sspan]
    p.add_argument('--ekx',    type=float, default=0.078) # eye->screen proj, x (6.3cm/81cm)
    p.add_argument('--eky',    type=float, default=0.137) # eye->screen proj, y (6.3cm/46cm)
    p.add_argument('--ey0',    type=float, default=0.0)   # camera axis screen y (0 = top)
    p.add_argument('--fraction',  type=float, default=1.0)   # train on this fraction of frames
    p.add_argument('--volume',    type=int,   default=1)     # ingest volume_look/volume_head folders
    p.add_argument('--vel_tol',   type=float, default=0.02)  # max detection deviation vs smoothed track
    p.add_argument('--model',     default='look', choices=['look', 'target', 'refine', 'gate'])
    p.add_argument('--rwin',      type=int,   default=4)     # refine: jittered windows per frame
    p.add_argument('--balance',   type=int,   default=1)     # look: equalize screen-gaze bin sampling
    p.add_argument('--bins_x',    type=int,   default=16)    # balance grid: 16x9 screen areas
    p.add_argument('--bins_y',    type=int,   default=9)
    p.add_argument('--annotate',  action='store_true')      # plot eyes on unannotated frames (torch models)
    p.add_argument('--process',   default='', choices=['', 'base', 'volumes', 'look'])
    p.add_argument('--export_ts', action='store_true')      # trace models -> app .ptc files
    p.add_argument('--group_by',  default='headdir', choices=['circle', 'pose', 'headdir'])  # eval holdout unit
    return p.parse_args()

args = parse_args()


def read_pair(agi_text, key):
    # scale entries carry a single value; y is optional
    m = re.search(rf'{re.escape(key)}:\s*(-?[\d.]+)(?:[ \t]+(-?[\d.]+))?', agi_text)
    if not m:
        return (-1.0, -1.0)
    return (float(m.group(1)), float(m.group(2)) if m.group(2) else 0.0)


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


def load_session(session_dir):
    # cached: PNG decode + crops in one .npz, keyed by .agi mtimes
    cache = os.path.join(session_dir, f'.torchcache-{args.size}-c{args.cams}-e{args.eye_div}-v17.npz')
    dirs = [session_dir] + [os.path.join(session_dir, s)
                            for s in ('volume_look', 'volume_head',
                                      'applied_look', 'applied_head')
                            if os.path.isdir(os.path.join(session_dir, s))]
    newest = max((os.path.getmtime(os.path.join(d, f))
                  for d in dirs for f in os.listdir(d) if f.endswith('.agi')), default=0)
    if os.path.exists(cache) and os.path.getmtime(cache) >= newest:
        z = np.load(cache)
        return [z[k] for k in ('tl', 'tr', 'tf')], z['laux'], z['ly'], z['lg'], z['lfid']
    from PIL import Image
    print(f'building crop cache at size {args.size} ...')
    look = {'tl': [], 'tr': [], 'tf': []}
    laux, ly, lg, lfid = [], [], [], []
    groups = {}
    frame = 0
    while True:
        agi = os.path.join(session_dir, f'{frame}.agi')
        if not os.path.exists(agi):
            break
        text = open(agi).read()
        center = read_pair(text, 'head')
        if center[0] <= -0.5:
            center = read_pair(text, 'center')
        offset = read_pair(text, 'offset')
        lkm    = read_pair(text, 'look')
        if lkm[0] <= -0.5:
            lkm = (center[0] + offset[0], center[1] + offset[1])
        left   = read_pair(text, 'top_left')
        right  = read_pair(text, 'top_right')
        scale  = read_pair(text, 'top_scale')
        png    = os.path.join(session_dir, f'{frame}-top.png')
        if (center[0] >= 0.0 and left[0] >= -100.0 and right[0] >= -100.0
                and scale[0] > 0.0 and os.path.exists(png)
                and left[0] != -1.0 and right[0] != -1.0
                and not eyes_off_frame(left, right)):
            gray = np.asarray(Image.open(png).convert('L'))
            side = max(scale[0] / args.eye_div, 0.03)
            med  = ((left[0] + right[0]) * 0.5, (left[1] + right[1]) * 0.5)
            look['tl'].append(crop_tensor(gray, left[0],  left[1],  side))
            look['tr'].append(crop_tensor(gray, right[0], right[1], side))
            look['tf'].append(crop_tensor(gray, med[0], med[1], max(scale[0] * 2.0, 0.1)))
            laux.append([med[0], med[1], scale[0],
                         left[0], left[1], right[0], right[1]])
            ly.append([center[0], center[1], lkm[0], lkm[1]])
            gk = (round(center[0], 5), round(center[1], 5))
            lg.append(groups.setdefault(gk, len(groups)))
            lfid.append(frame)
        frame += 1
    # volume folders: sequential captures; the head moves slowly, so
    # detections that jump off the median-smoothed track are dropped
    if args.volume:
        from PIL import Image
        for si, sub in enumerate(('look', 'head')):
            vd = os.path.join(session_dir, 'volume_' + sub)
            ad = os.path.join(session_dir, 'applied_' + sub)
            seq = []
            i = 0
            while os.path.exists(os.path.join(ad, f'{i}.agi')):
                text = open(os.path.join(ad, f'{i}.agi')).read()
                if 'off_screen: true' in text:
                    i += 1
                    continue
                center = read_pair(text, 'head')
                if center[0] <= -0.5:
                    center = read_pair(text, 'center')
                offset = read_pair(text, 'offset')
                lkv = read_pair(text, 'look')
                left   = read_pair(text, 'top_left')
                right  = read_pair(text, 'top_right')
                scale  = read_pair(text, 'top_scale')
                png    = os.path.join(vd, f'{i}-top.png')
                if lkv[0] <= -0.5:
                    lkv = (center[0] + offset[0], center[1] + offset[1])
                if center[0] <= -0.5:
                    center = lkv
                if (center[0] >= 0.0 and left[0] > -900.0 and right[0] > -900.0
                        and scale[0] > 0.0 and os.path.exists(png)
                        and not eyes_off_frame(left, right)):
                    seq.append((i, center, lkv, left, right, scale[0], png))
                i += 1
            if len(seq) < 5:
                continue
            med = np.array([[(s[3][0] + s[4][0]) / 2, (s[3][1] + s[4][1]) / 2]
                            for s in seq])
            k5 = 5
            sm = med.copy()
            for j in range(len(med)):
                a = max(0, j - k5 // 2)
                sm[j] = np.median(med[a:a + k5], axis=0)
            ok = np.abs(med - sm).max(1) < args.vel_tol
            kept = 0
            for j, s in enumerate(seq):
                if not ok[j]:
                    continue
                fi, center, offset, left, right, sc, png = s
                gray = np.asarray(Image.open(png).convert('L'))
                side = max(sc / args.eye_div, 0.03)
                m2 = ((left[0] + right[0]) * 0.5, (left[1] + right[1]) * 0.5)
                look['tl'].append(crop_tensor(gray, left[0],  left[1],  side))
                look['tr'].append(crop_tensor(gray, right[0], right[1], side))
                look['tf'].append(crop_tensor(gray, m2[0], m2[1], max(sc * 2.0, 0.1)))
                laux.append([m2[0], m2[1], sc,
                             left[0], left[1], right[0], right[1]])
                gz = offset   # slot carries the absolute look now
                ly.append([center[0], center[1], gz[0], gz[1]])
                gk = (round(gz[0], 5), round(gz[1], 5))
                lg.append(groups.setdefault(gk, len(groups)))
                lfid.append(100000 * (si + 1) + fi)
                kept += 1
            print(f'{sub}: {kept} kept / {len(seq)} annotated ({int((~ok).sum())} velocity-rejected)')
    r = ([np.array(look[k], np.float32) for k in ('tl', 'tr', 'tf')],
         np.array(laux, np.float32), np.array(ly, np.float32),
         np.array(lg), np.array(lfid))
    np.savez_compressed(cache, tl=r[0][0], tr=r[0][1], tf=r[0][2],
                        laux=r[1], ly=r[2], lg=r[3], lfid=r[4])
    return r


class ImgEnc(nn.Module):
    # size x size x 1 -> 64 features; shared between both eyes
    def __init__(self):
        super().__init__()
        s = args.size // 4
        self.net = nn.Sequential(
            nn.Conv2d(1, 16, 3, padding=1), nn.ReLU(),
            nn.MaxPool2d(2),
            nn.Conv2d(16, 32, 3, padding=1), nn.ReLU(),
            nn.MaxPool2d(2),
            nn.Flatten(),
            nn.Linear(s * s * 32, 64), nn.ReLU())

    def forward(self, x):
        return self.net(x)


class EyeEnc(nn.Module):
    # hybrid: sub-pixel soft-argmax coordinates (no pooling on that
    # path — a 1px iris shift moves them directly) PLUS pooled
    # appearance features for context. 24 + 64 = 88 per eye.
    def __init__(self, k=8):
        super().__init__()
        self.k = k
        self.trunk = nn.Sequential(
            nn.Conv2d(1, 32, 3, padding=1), nn.ReLU(),
            nn.Conv2d(32, 32, 3, padding=1), nn.ReLU())
        self.heat = nn.Conv2d(32, k, 1)
        s = args.size // 4
        self.app = nn.Sequential(
            nn.MaxPool2d(2),
            nn.Conv2d(32, 32, 3, padding=1), nn.ReLU(),
            nn.MaxPool2d(2),
            nn.Flatten(),
            nn.Linear(s * s * 32, 64), nn.ReLU())
        lin = torch.linspace(0.0, 1.0, args.size)
        self.register_buffer('lin', lin)

    def forward(self, x):
        t = self.trunk(x)
        hm = self.heat(t)                      # B,k,S,S
        B, k, S, _ = hm.shape
        p = torch.softmax(hm.reshape(B, k, -1) * 8.0, -1).reshape(B, k, S, S)
        xs = (p.sum(2) * self.lin).sum(-1)     # B,k sub-pixel x
        ys = (p.sum(3) * self.lin).sum(-1)     # B,k sub-pixel y
        pk = hm.reshape(B, k, -1).max(-1).values * 0.1   # confidence
        return torch.cat([xs, ys, pk, self.app(t)], 1)   # B, 3k+64


class CenterNet(nn.Module):
    def __init__(self):
        super().__init__()
        self.eye  = EyeEnc()                   # shared, 24 feats/eye
        self.face = ImgEnc()
        self.aux  = nn.Sequential(nn.Linear(7, 64), nn.ReLU(),
                                  nn.Linear(64, 64), nn.ReLU())
        self.center = nn.Sequential(nn.Linear(128, 64), nn.ReLU(),
                                    nn.Dropout(args.dropout),
                                    nn.Linear(64, 2))
        self.gamma = nn.Linear(64, 240)
        self.beta  = nn.Linear(64, 240)
        self.delta = nn.Sequential(nn.Linear(240 + 64, 64), nn.ReLU(),
                                   nn.Dropout(args.dropout),
                                   nn.Linear(64, 2))

    def forward(self, l, r, f, a):
        el = self.eye(l)
        er = self.eye(torch.flip(r, [3]))   # mirror: shared chirality
        ef = self.face(f)
        ea = self.aux(a)
        center = self.center(torch.cat([ef, ea], 1))
        feats  = torch.cat([el, er, ef], 1)    # 88+88+64 = 240
        feats  = feats * (1.0 + torch.tanh(self.gamma(ea))) + self.beta(ea)
        delta  = self.delta(torch.cat([feats, ea], 1))
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
    i = 0
    while os.path.exists(os.path.join(session_dir, f'{i}.agi')):
        text = open(os.path.join(session_dir, f'{i}.agi')).read()
        left, right = read_pair(text, 'top_left'), read_pair(text, 'top_right')
        scale = read_pair(text, 'top_scale')
        png = os.path.join(session_dir, f'{i}-top.png')
        if left[0] > -900 and right[0] > -900 and scale[0] > 0 and os.path.exists(png):
            entries.append((png, [left[0], left[1], right[0], right[1], scale[0]]))
        i += 1
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
        print(f'{sub}: {int(ok.sum())} kept / {len(seq)} ({int((~ok).sum())} velocity-rejected)')
    print(f'plot frames: {len(entries)} total ({n_gold} gold)')
    return entries


def build_plot_cache(session_dir):
    tag = args.model
    cache = os.path.join(session_dir, f'.torchcache-{tag}-{args.size}-w{args.rwin}-v10.npz')
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
    xs, ys = [], []
    for png, lab in entries:
        gray = np.asarray(Image.open(png).convert('L'))
        if args.model == 'target':
            xs.append(crop_tensor(gray, 0.0, 0.0, 0.0))
            ys.append(lab)
            # 4x: view shifted up to +/-20%, black backfill — the head
            # goes off screen and the net keeps placing it from
            # neck/shoulder context
            h2, w2 = gray.shape
            for _ in range(3):
                sx = int((np.random.rand() - 0.5) * 0.4 * w2)
                sy = int((np.random.rand() - 0.5) * 0.4 * w2)
                sh2 = np.zeros_like(gray)
                x0s, x1s = max(0, sx), min(w2, w2 + sx)
                y0s, y1s = max(0, sy), min(h2, h2 + sy)
                sh2[y0s:y1s, x0s:x1s] = gray[y0s - sy:y1s - sy, x0s - sx:x1s - sx]
                xs.append(crop_tensor(sh2, 0.0, 0.0, 0.0))
                dx, dy = sx / w2, sy / w2
                # centroid within half a face-scale of an edge: the
                # face is effectively leaving the view -> off_screen
                cxs = (lab[0] + lab[2]) / 2 + dx
                cys = (lab[1] + lab[3]) / 2 + dy
                hs = lab[4] / 2
                offl = 1.0 if (cxs < hs or cxs > 1 - hs
                               or cys < hs or cys > 1 - hs) else 0.0
                ys.append([lab[0] + dx, lab[1] + dy,
                           lab[2] + dx, lab[3] + dy, lab[4], offl])
        else:
            lx, lyv, rx, ry, sc = lab
            for _ in range(args.rwin):
                ws = max(sc * 1.25 * (0.6 + np.random.rand() * 0.9), 0.15)
                cx = (lx + rx) / 2 + (np.random.rand() - 0.5) * 0.2
                cy = (lyv + ry) / 2 - ws * 0.1 + (np.random.rand() - 0.5) * 0.2
                wl = [(lx - (cx - ws / 2)) / ws, (lyv - (cy - ws / 2)) / ws,
                      (rx - (cx - ws / 2)) / ws, (ry - (cy - ws / 2)) / ws, sc / ws]
                # eyes MAY sit outside the window (face part-occluded at
                # a frame edge): refine must learn to point beyond its
                # own bounds instead of dragging features inward
                if not all(-0.5 <= v <= 1.5 for v in wl[:4]):
                    continue
                xs.append(crop_tensor(gray, cx, cy, ws))
                ys.append(wl)
    # 6th column: off_screen. domain frames 0; volume_off frames 1
    # (coords masked) — a second detector besides the gate. shifted
    # variants may already carry their own off label
    ys = [lab + [0.0] if len(lab) == 5 else lab for lab in ys]
    if args.model == 'target':
        nd = os.path.join(session_dir, 'volume_off')
        neg = 0
        i = 0
        while os.path.exists(os.path.join(nd, f'{i}.agi')):
            png = os.path.join(nd, f'{i}-top.png')
            i += 1
            if not os.path.exists(png):
                continue
            gray = np.asarray(Image.open(png).convert('L'))
            xs.append(crop_tensor(gray, 0.0, 0.0, 0.0))
            ys.append([0.5, 0.5, 0.5, 0.5, 0.2, 1.0])
            neg += 1
        if neg:
            print(f'volume_off: {neg} off-screen negatives')
    x = np.array(xs, np.float32)
    y = np.array(ys, np.float32)
    np.savez_compressed(cache, x=x, y=y)
    return x, y


class PlotNet(nn.Module):
    # eye plotter: no-pool soft-argmax coordinates + pooled appearance
    def __init__(self):
        super().__init__()
        k = 8
        self.trunk = nn.Sequential(
            nn.Conv2d(1, 32, 3, padding=1), nn.ReLU(),
            nn.Conv2d(32, 32, 3, padding=1), nn.ReLU())
        self.heat = nn.Conv2d(32, k, 1)
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
        t = self.trunk(x)
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
    gp = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models', 'gate_torch.pt')
    gate = None
    if os.path.exists(gp):
        gate = GateNet().to('cuda' if torch.cuda.is_available() else 'cpu')
        gate.load_state_dict(torch.load(gp))
        gate.eval()
    for nm in ('target', 'refine'):
        m = PlotNet().to(dev)
        m.load_state_dict(torch.load(os.path.join(here, 'models', f'{nm}_torch.pt'),
                                     map_location=dev))
        m.eval()
        nets[nm] = m
    # face-focus estimation for volume_look needs an initial look model
    look_net = None
    lp = os.path.join(here, 'models', 'look_torch.pt')
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
                    if not (0.0 <= fcx0 <= 1.0 and 0.0 <= fcy0 <= 1.0):
                        continue   # face centroid off screen: unplotted
                    ws = max(sc * 1.25, 0.15)
                    cx = (lx + rx) / 2
                    cy = (lyv + ry) / 2 - ws * 0.1
                    rwin = crop_tensor(gray, cx, cy, ws)
                    r5 = nets['refine'](nchw(rwin[None]).to(dev))[0].cpu().numpy()
                    ox, oy = cx - ws / 2, cy - ws / 2
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
                    if dot[0] <= -0.5:
                        dot = read_pair(raw, 'center')
                    side = max(sc / args.eye_div, 0.03)
                    m2 = ((lx + rx) / 2, (lyv + ry) / 2)
                    li = crop_tensor(gray, lx, lyv, side)
                    ri = crop_tensor(gray, rx, ry, side)
                    fi = crop_tensor(gray, m2[0], m2[1], max(sc * 2.0, 0.1))
                    av = np.array([[m2[0], m2[1], sc, lx, lyv, rx, ry]], np.float32)
                    c, g = look_net(nchw(li[None]).to(dev), nchw(ri[None]).to(dev),
                                    nchw(fi[None]).to(dev), torch.from_numpy(av).to(dev))
                    fcx, fcy = float(c[0, 0]), float(c[0, 1])
                    agi_put(ap, '    head', f'{fcx} {fcy}')
                    agi_put(ap, '    look', f'{dot[0]} {dot[1]}')
                    faced += 1
    print(f'annotate: {done} frames eye-plotted, {faced} face-focus estimated, {gated} gate-excluded')


class GateNet(nn.Module):
    # face-usable vs off-camera: binary gate on the full frame
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


def train_gate():
    from PIL import Image
    sd = f'/src/hyperspace-sessions/{args.session}'
    cache = os.path.join(sd, f'.torchcache-gate-{args.size}-v1.npz')
    dirs_pos = [sd] + [os.path.join(sd, s) for s in ('volume_look', 'volume_head')
                       if os.path.isdir(os.path.join(sd, s))]
    nd = os.path.join(sd, 'volume_off')
    all_dirs = dirs_pos + ([nd] if os.path.isdir(nd) else [])
    newest = max((os.path.getmtime(os.path.join(d, f))
                  for d in all_dirs for f in os.listdir(d) if f.endswith('.agi')), default=0)
    if os.path.exists(cache) and os.path.getmtime(cache) >= newest:
        z = np.load(cache)
        x, y = z['x'], z['y']
    else:
        print('building gate cache ...')
        xs, ys = [], []
        for d in all_dirs:
            lab = 0.0 if d.endswith('volume_off') else 1.0
            i = 0
            while os.path.exists(os.path.join(d, f'{i}.agi')):
                png = os.path.join(d, f'{i}-top.png')
                i += 1
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
    print(f'gate: {n - n_neg} domain / {n_neg} off-camera')
    order = np.random.permutation(n)
    n_ev = max(1, n // 10)
    ev, tr = order[:n_ev], order[n_ev:]
    dev = 'cuda' if torch.cuda.is_available() else 'cpu'
    m = GateNet().to(dev)
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
        torch.save(best_state, os.path.join(out, 'gate_torch.pt'))
        print(f'best eval acc {best:.4f} -> {out}/gate_torch.pt')
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
    xtr = nchw(x[tr]).to(dev)
    ytr = torch.from_numpy(y[tr]).to(dev)
    xev = nchw(x[ev]).to(dev)
    yev = torch.from_numpy(y[ev]).to(dev)
    cols = ['left.x', 'left.y', 'right.x', 'right.y', 'scale', 'off.err']
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
            coord = (((p[:, :5] - ytr[idx, :5]) ** 2) * pm).sum() / (pm.sum() * 5 + 1e-6)
            offb  = nn.functional.binary_cross_entropy_with_logits(p[:, 5], ytr[idx, 5])
            loss = coord + 0.2 * offb
            opt.zero_grad()
            loss.backward()
            opt.step()
            tot += float(coord.detach()) * len(idx)
        m.eval()
        with torch.no_grad():
            p = m(xev)
            onm = yev[:, 5] < 0.5
            eval_loss = float(((p[onm, :5] - yev[onm, :5]) ** 2).mean())
            errs = (p[onm, :5] - yev[onm, :5]).abs().mean(0).cpu().numpy()
            oacc = float(((p[:, 5] > 0) == (yev[:, 5] > 0.5)).float().mean())
            errs = np.concatenate([errs, [1.0 - oacc]])
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
        torch.save(best_state, os.path.join(out, f'{args.model}_torch.pt'))
        print(f'best epoch {best_ep} (avg err {best:.6g}) -> {out}/{args.model}_torch.pt')
    print('training complete')


def export_ts():
    # trace trained models for the app's libtorch runtime
    here = os.path.dirname(os.path.abspath(__file__))
    out = '/src/silver/platform/native/share/hyperspace/models'
    os.makedirs(out, exist_ok=True)

    class LookWrap(nn.Module):
        def __init__(self, m):
            super().__init__()
            self.m = m

        def forward(self, l, r, f, a):
            c, g = self.m(l, r, f, a)
            return torch.cat([c, g], 1)

    S = args.size
    jobs = [('target_torch.pt', PlotNet, 'target.ptc'),
            ('refine_torch.pt', PlotNet, 'refine.ptc'),
            ('gate_torch.pt', GateNet, 'gate.ptc'),
            ('look_final_torch.pt', CenterNet, 'look_final.ptc')]
    for src, cls, dst in jobs:
        p = os.path.join(here, 'models', src)
        if not os.path.exists(p):
            print(f'export_ts: {src} missing, skipped')
            continue
        m = cls()
        m.load_state_dict(torch.load(p, map_location='cpu'))
        m.eval()
        if cls is CenterNet:
            m = LookWrap(m)
            ex = (torch.zeros(1, 1, S, S), torch.zeros(1, 1, S, S),
                  torch.zeros(1, 1, S, S), torch.zeros(1, 7))
        else:
            ex = (torch.zeros(1, 1, S, S),)
        ts = torch.jit.trace(m, ex)
        ts.save(os.path.join(out, dst))
        print(f'exported {dst} -> {out}')


def main():
    assert args.cams == 1, 'torch trainer is single-cam'
    if args.seed:
        np.random.seed(args.seed)
        torch.manual_seed(args.seed)
    if args.export_ts:
        export_ts()
        return
    if args.process == 'base':
        # process 1: the base set from gold annotations — target,
        # refine, and the gold-only look (volumes needs it for
        # face-focus estimation)
        for nm in ('target', 'refine'):
            args.model = nm
            print(f'== base: training {nm} ==')
            train_plot()
        if os.path.isdir(f'/src/hyperspace-sessions/{args.session}/volume_off'):
            print('== base: training gate (domain vs off-camera) ==')
            train_gate()
        args.model = 'look'
        args.volume = 0
        print('== base: training look (gold only) ==')
        train_look()
        return
    if args.process == 'volumes':
        # process 2: apply ops over both volumes — eye plots + face
        # focus (base models required)
        annotate_frames()
        return
    if args.process == 'look':
        # process 3: final look with the applied volumes included,
        # then export ALL app models so the testbed is never stale
        args.volume = 1
        train_look()
        export_ts()
        return
    if args.annotate:
        annotate_frames()
        return
    if args.model == 'gate':
        train_gate()
        return
    if args.model in ('target', 'refine'):
        train_plot()
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
    if args.group_by == 'headdir':
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
        for col in (0, 1, 3, 4, 5, 6):
            a2[:, col] = 0.5 + (a2[:, col] - 0.5) * f
        a2[:, 2] = base_a[:, 2] * f
        s0 = base_a[:, 2]
        Ex = 0.5 + args.ekx * (base_a[:, 0] - 0.5) / s0
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
        for col in (0, 3, 5): a2[:, col] += dx
        for col in (1, 4, 6): a2[:, col] += dy
        sc = taux[bi][:, 2]
        y2 = tly[bi].copy()
        y2[:, 0] += args.tkx * dx / sc
        y2[:, 2] += args.tkx * dx / sc
        y2[:, 1] += args.tky * dy / sc
        y2[:, 3] += args.tky * dy / sc
        timgs = [np.concatenate([timgs[k], timgs[k][bi]]) for k in range(3)]
        taux  = np.concatenate([taux, a2]).astype(np.float32)
        tly   = np.concatenate([tly, y2]).astype(np.float32)
    print(f'look: {len(tly)} train / {len(ev)} eval ({args.group_by} holdout)')

    dev = 'cuda' if torch.cuda.is_available() else 'cpu'
    m = CenterNet().to(dev)
    opt = (torch.optim.AdamW(m.parameters(), lr=args.lr, weight_decay=args.wd)
           if args.optimizer == 'adam'
           else torch.optim.SGD(m.parameters(), lr=args.lr, weight_decay=args.wd))

    xtr = [nchw(timgs[k]).to(dev) for k in range(3)]
    atr = torch.from_numpy(taux).to(dev)
    ytr = torch.from_numpy(tly).to(dev)
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
        g5 = tly[:, 2:4]
        bx = np.clip((g5[:, 0] * args.bins_x).astype(int), 0, args.bins_x - 1)
        by = np.clip((g5[:, 1] * args.bins_y).astype(int), 0, args.bins_y - 1)
        bid = by * args.bins_x + bx
        cnt = np.bincount(bid, minlength=args.bins_x * args.bins_y).astype(np.float64)
        w5 = 1.0 / cnt[bid]
        bw = torch.from_numpy((w5 / w5.sum()).astype(np.float32)).to(dev)
        occ = cnt[cnt > 0]
        print(f'balance: {len(occ)} occupied bins, counts {int(occ.min())}..{int(occ.max())}')
    cols = ['head.x', 'head.y', 'gaze.x', 'gaze.y']
    print(pad('epoch', 10) + pad('train') + pad('eval') +
          ''.join(pad(c) for c in cols))
    best, best_state, best_ep = None, None, 0
    for epoch in range(args.epochs):
        m.train()
        if bw is not None:
            order = torch.multinomial(bw, n, replacement=True)
        else:
            order = torch.randperm(n, device=dev)
        tot = 0.0
        for b in range(0, n, bs):
            idx = order[b:b + bs]
            xb = [x[idx] + torch.randn_like(x[idx]) * args.noise for x in xtr]
            ab = atr[idx] + torch.randn_like(atr[idx]) * args.aux_noise
            c, g = m(*xb, ab)
            # delta supervised directly against the measured eye offset:
            # keeps the eyes' head from absorbing center errors
            d_true = ytr[idx, 2:4] - ytr[idx, :2]
            loss = (((c - ytr[idx, :2]) ** 2).mean()
                    + 2.0 * ((g - ytr[idx, 2:4]) ** 2).mean()
                    + 2.0 * (((g - c) - d_true) ** 2).mean())
            opt.zero_grad()
            loss.backward()
            opt.step()
            # report plain mse so train and eval columns are comparable
            plain = float(((torch.cat([c, g], 1) - ytr[idx]) ** 2).mean().detach())
            tot += plain * len(idx)
        m.eval()
        with torch.no_grad():
            c, g = m(*xev, aev)
            p = torch.cat([c, g], 1)
            eval_loss = float(((p - yev) ** 2).mean())
            errs = (p - yev).abs().mean(0).cpu().numpy()
        print(pad(f'{epoch + 1}/{args.epochs}', 10) +
              pad(f'{tot / n:.6g}') + pad(f'{eval_loss:.6g}') +
              ''.join(pad(f'{e:.6g}') for e in errs))
        mval = float(errs.mean())
        if best is None or mval < best:
            best, best_ep = mval, epoch + 1
            best_state = {k: v.clone() for k, v in m.state_dict().items()}
    if best_state is not None:
        m.load_state_dict(best_state)
        out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models')
        os.makedirs(out, exist_ok=True)
        # the volumes process trains the gold-only bootstrap look;
        # the final process (applied volumes included) is look_final
        nm = 'look_final_torch.pt' if args.process == 'look' else 'look_torch.pt'
        torch.save(best_state, os.path.join(out, nm))
        print(f'best epoch {best_ep} (avg err {best:.6g}) -> {os.path.join(out, nm)}')
    print('training complete')


if __name__ == '__main__':
    main()
