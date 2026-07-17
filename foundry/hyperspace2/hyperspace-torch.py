#!/usr/bin/env python3
# torch twin of hyperspace2.py — identical loader, cache, labels, and
# architecture (split aux head, soft-argmax encoders, sensor noise), so
# the two frameworks can be A/B'd on the exact same data. samples are
# {i}.agi + {i}-face{c}.png; eye/face crops are cut at the EXACT label
# plots (no detection, no augmentation — the eye crops are never jittered).
#
# label conventions (zero-centered, -0.5 left/top .. +0.5 right/bottom):
#   look / head        screen plots
#   face_left/right    resting eye-socket centers in the face image
#   face_*_oc          outer eye corners in the face image
#   face_scale         inter-eye distance, image-width fraction
# pixel position in the saved image = (plot + 0.5) * dims
import argparse, os, re, hashlib
import numpy as np
import torch
import torch.nn as nn


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument('--session',  default='default')
    p.add_argument('--epochs',   type=int,   default=30)
    p.add_argument('--lr',       type=float, default=0.0001)
    p.add_argument('--batch',    type=int,   default=64)
    p.add_argument('--draw',     type=int,   default=20000)  # samples drawn per epoch
    p.add_argument('--size',     type=int,   default=32)
    p.add_argument('--seed',     type=int,   default=1234)
    # AdamW decoupled weight decay; 0 = plain Adam behavior
    p.add_argument('--wd',       type=float, default=0.01)
    # extra eval columns on the EASY subset: head within 25% of center,
    # gaze within 0.25 of the head's own screen point
    p.add_argument('--easy_eval', action='store_true')
    # optics blur on the GENERATED frames at load: gaussian radius drawn
    # per sample between HALF of this and this, as a fraction of the
    # frame side (0.11%..0.22% of the image). baked into the cache; 0 = off
    p.add_argument('--blur', type=float, default=0.0022)
    p.add_argument('--eye_div',  type=float, default=2.12)   # eye crop side = scale/eye_div
    p.add_argument('--face_mul', type=float, default=1.75)   # face crop side = scale*face_mul
    # cam_rel{c} = head angle off that camera's axis, degrees; a view
    # past this is dropped (the pair's other camera still trains). <=0 = off
    p.add_argument('--cam_rel_max', type=float, default=60.0)
    # the cascade: 'target' finds the eye from a sloppy crop, 'refine'
    # polishes from look's own crop convention, 'look' reads gaze
    p.add_argument('--process',  default='look', choices=['look', 'target', 'refine', 'offset'])
    # train-time aux jitter: keeps the geometry signal, kills the
    # per-sample fingerprint (and matches the stabilizer's runtime error)
    p.add_argument('--aux_noise', type=float, default=0.0)
    # train-time sensor model: per-sample gain jitter + per-pixel gaussian
    # noise. renders are exact, so without this the net memorizes pixels
    p.add_argument('--px_noise', type=float, default=0.02)
    p.add_argument('--zero_aux', action='store_true')  # zero aux everywhere
    # zero ONLY the face meter offset relative to screen (aux 21..23),
    # keeping the image-plane geometry and head rotation
    p.add_argument('--zero_offset', action='store_true')
    # zero the face CROP image (the head_img correction sees black)
    p.add_argument('--zero_face', action='store_true')
    # DIAGNOSTIC ONLY: feed the ground-truth ray (head rotation + meter
    # offset, aux 18..23) — unavailable at runtime, so never deployable
    p.add_argument('--ray_aux', action='store_true')
    p.add_argument('--stats',    action='store_true')
    p.add_argument('--export_ts', action='store_true')  # trace models -> app .ptc files
    p.add_argument('--preview',  action='store_true')
    return p.parse_args()

args = parse_args()


def read_pair(text, key):
    # scientific notation happens whenever a plot lands near zero —
    # '7e-05' read as '7' poisoned whole rows before this pattern
    m = re.search(rf'{re.escape(key)}:\s*(-?[\d.]+(?:[eE][-+]?\d+)?)(?:[ \t]+(-?[\d.]+(?:[eE][-+]?\d+)?))?', text)
    if not m:
        return (-999.0, -999.0)
    return (float(m.group(1)), float(m.group(2)) if m.group(2) else 0.0)


def crop_gray(gray, cx, cy, side, out):
    # square crop centered at (cx, cy); out-of-frame padded, never clamped
    H, W = gray.shape
    s = max(4, int(side))
    x0 = int(round(cx - s / 2)); y0 = int(round(cy - s / 2))
    patch = np.zeros((s, s), np.float32)
    dx0 = max(0, x0); dy0 = max(0, y0)
    dx1 = min(W, x0 + s); dy1 = min(H, y0 + s)
    if dx1 > dx0 and dy1 > dy0:
        patch[dy0 - y0:dy1 - y0, dx0 - x0:dx1 - x0] = gray[dy0:dy1, dx0:dx1]
    from PIL import Image
    c = Image.fromarray((patch * 255).astype(np.uint8))
    arr = np.asarray(c.resize((out, out), Image.BOX), np.float32)[..., None] / 255.0
    return arr, x0, y0, s


def read3(text, key):
    m = re.search(rf'{re.escape(key)}:\s*(-?[\d.eE+-]+)\s+(-?[\d.eE+-]+)\s+(-?[\d.eE+-]+)', text)
    return [float(m.group(i)) for i in (1, 2, 3)] if m else [0.0, 0.0, 0.0]


def load_one(job):
    # one job = one .agi; returns one training sample PER CAMERA view
    session_dir, fid = job
    text = open(os.path.join(session_dir, f'{fid}.agi')).read()
    cams = int(read_pair(text, 'cameras')[0])
    if cams <= 0:
        # legacy single-camera .agi: unsuffixed fields, {i}-face.png
        return [s for s in [load_cam(session_dir, fid, text, '', '')] if s]
    out = []
    for c in range(cams):
        s = load_cam(session_dir, fid, text, str(c), str(c))
        if s:
            out.append(s)
    return out


def load_cam(session_dir, fid, text, sfx, img_sfx):
    from PIL import Image, ImageDraw
    head = read_pair(text, 'head')
    look = read_pair(text, 'look')
    el   = read_pair(text, f'face_left{sfx}')
    er   = read_pair(text, f'face_right{sfx}')
    ocl  = read_pair(text, f'face_left_oc{sfx}')
    ocr  = read_pair(text, f'face_right_oc{sfx}')
    head_rotation = read3(text, 'head_rot')
    face_offset   = read3(text, 'face_off')
    sc   = read_pair(text, f'face_scale{sfx}')[0]
    if look[0] < -900 or el[0] < -900 or sc <= 0:
        return None
    # extreme relative view on this camera: drop the row, keep the pair
    crel = read_pair(text, f'cam_rel{sfx}')[0]
    if args.cam_rel_max > 0 and crel > args.cam_rel_max:
        return None
    # eye_off = the eye coordinate (oc-ic midpoint per eye, between
    # both eyes) relative to the SCREEN CENTER, meters, person frame.
    # absent on older .agis: -1000 sentinels, offset rows masked out
    if 'eye_off:' in text:
        eoff = read3(text, 'eye_off')
    else:
        eoff = [-1000.0, -1000.0, -1000.0]
    img = Image.open(os.path.join(session_dir, f'{fid}-face{img_sfx}.png')).convert('L')
    if args.blur > 0:
        # per-sample optics defocus on the WHOLE generated frame, before
        # any crop: deterministic per (sample, camera) so cache rebuilds
        # reproduce; radius in fractions of the frame side
        from PIL import ImageFilter
        brng = np.random.RandomState(fid * 31 + (int(img_sfx) if img_sfx else 0))
        brad = (args.blur / 2.0 + brng.rand() * (args.blur / 2.0)) * img.width
        img = img.filter(ImageFilter.GaussianBlur(brad))
    gray = np.asarray(img, np.float32) / 255.0
    H, W = gray.shape
    # plots -> pixels: p = (plot + 0.5) * dims
    lx, lyv = (el[0] + 0.5) * W, (el[1] + 0.5) * H
    rx, ryv = (er[0] + 0.5) * W, (er[1] + 0.5) * H
    side = sc * W / args.eye_div
    lc, lx0, ly0, ls = crop_gray(gray, lx, lyv, side, args.size)
    rc, rx0, ry0, rs = crop_gray(gray, rx, ryv, side, args.size)
    fx, fy = (lx + rx) / 2, (lyv + ryv) / 2
    fc, fx0, fy0, fs = crop_gray(gray, fx, fy, sc * W * args.face_mul, args.size)
    # target/refine cascade crops: the eye seen at a jittered size and
    # offset; target sweeps wide, refine trains on look's own crop
    # convention (side = scale/eye_div) with only the residual error
    # target leaves. right eye mirrored so one net serves both. labels
    # per eye: center offset in-crop + log correction to the true side
    jrng = np.random.RandomState(fid * 131 + (int(img_sfx) if img_sfx else 0) * 7 + 5)
    def stage_crop(ex, ey, mul_lo, mul_hi, off):
        sj = side * jrng.uniform(mul_lo, mul_hi)
        cx = ex + jrng.uniform(-off, off) * sj
        cy = ey + jrng.uniform(-off, off) * sj
        c, x0, y0, s0 = crop_gray(gray, cx, cy, sj, args.size)
        dx = (ex - (x0 + s0 / 2.0)) / s0
        dy = (ey - (y0 + s0 / 2.0)) / s0
        return c, dx, dy, float(np.log(side / s0)), x0, y0, s0
    # both stages get EXPLODED: 10 independent framings per eye, each
    # its own training row — the localizers see the eye every which way.
    # refine ALSO learns each eye's outer corner in-crop, so every
    # runtime aux slot look consumes comes from refine
    oclx, ocly = (ocl[0] + 0.5) * W, (ocl[1] + 0.5) * H
    ocrx, ocry = (ocr[0] + 0.5) * W, (ocr[1] + 0.5) * H
    tgs, tgls = [], []
    rfs, rfls = [], []
    for _ in range(10):
        tcl, tlx, tly, tls, _, _, _ = stage_crop(lx, lyv, 1.8, 4.0, 0.35)
        tcr, trx, tryy, trs, _, _, _ = stage_crop(rx, ryv, 1.8, 4.0, 0.35)
        tgs += [tcl, tcr[:, ::-1]]
        tgls += [tlx, tly, tls, -trx, tryy, trs]
        rcl, rlx, rly, rls, lx0r, ly0r, ls0r = stage_crop(lx, lyv, 0.9, 1.15, 0.08)
        rcr, rrx, rry, rrs, rx0r, ry0r, rs0r = stage_crop(rx, ryv, 0.9, 1.15, 0.08)
        lox = (oclx - (lx0r + ls0r / 2.0)) / ls0r
        loy = (ocly - (ly0r + ls0r / 2.0)) / ls0r
        rox = (ocrx - (rx0r + rs0r / 2.0)) / rs0r
        roy = (ocry - (ry0r + rs0r / 2.0)) / rs0r
        rfs += [rcl, rcr[:, ::-1]]
        rfls += [rlx, rly, lox, loy, rls, -rrx, rry, -rox, roy, rrs]
    tg  = np.concatenate(tgs, axis=2)
    rf  = np.concatenate(rfs, axis=2)
    tgl = np.array(tgls, np.float32)
    rfl = np.array(rfls, np.float32)
    # actual crop box: zero-centered center, side as image-width fraction
    def box(x0, y0, s):
        return [(x0 + s / 2) / W - 0.5, (y0 + s / 2) / H - 0.5, s / W]
    # head_rotation + face_offset = the origin-and-angle RAY: the head
    # label is a closed-form function of these six, so a net given them
    # must drive head error to ~0 or the trainer itself is broken
    aux = ([el[0], el[1], er[0], er[1],
            ocl[0], ocl[1], ocr[0], ocr[1], sc]
           + box(lx0, ly0, ls) + box(rx0, ry0, rs) + box(fx0, fy0, fs)
           + head_rotation + face_offset)
    y = [head[0], head[1], look[0], look[1], eoff[0], eoff[1], eoff[2]]
    # ALWAYS write {i}-preview.png beside the sample: the annotated frame
    # is the permanent validation of every label the net eats (512px —
    # the full-res encode was the slow part of loading)
    pim = img.convert('RGB')
    dr = ImageDraw.Draw(pim)
    def cross(x, y2, col, r2=4):
        px, py = (x + 0.5) * W, (y2 + 0.5) * H
        dr.line([px - r2, py, px + r2, py], fill=col, width=2)
        dr.line([px, py - r2, px, py + r2], fill=col, width=2)
    cross(el[0], el[1], (0, 255, 0))
    cross(er[0], er[1], (0, 255, 0))
    dr.rectangle([lx0, ly0, lx0 + ls, ly0 + ls], outline=(255, 64, 64), width=2)
    dr.rectangle([rx0, ry0, rx0 + rs, ry0 + rs], outline=(64, 128, 255), width=2)
    dr.rectangle([fx0, fy0, fx0 + fs, fy0 + fs], outline=(255, 255, 255), width=2)
    pim = pim.resize((512, 512), Image.BOX)
    dr = ImageDraw.Draw(pim)
    dr.text((6, 6), f'head {head[0]:+.3f} {head[1]:+.3f}  '
                    f'gaze {look[0]:+.3f} {look[1]:+.3f}  scale {sc:.3f}',
            fill=(0, 255, 0))
    pim.save(os.path.join(session_dir, f'{fid}-preview{img_sfx}.png'))
    # the FINISHED network inputs, side by side (left | right | face),
    # exactly as they go to the tensors — nearest-upscaled 4x so every
    # training pixel stays inspectable
    strip = np.concatenate([lc, rc, fc], axis=1)[:, :, 0]
    simg = Image.fromarray((strip * 255).astype(np.uint8))
    simg = simg.resize((simg.width * 4, simg.height * 4), Image.NEAREST)
    simg.save(os.path.join(session_dir, f'{fid}-inputs{img_sfx}.png'))
    return lc, rc, fc, tg, rf, aux, y, fid, tgl, rfl


def load_session(session_dir):
    ids = sorted(int(f[:-4]) for f in os.listdir(session_dir)
                 if f.endswith('.agi') and f[:-4].isdigit())
    if not ids:
        raise SystemExit(f'no .agi samples in {session_dir}')
    # folder mtime catches deletions too, not just newer samples
    newest = max(max(os.path.getmtime(os.path.join(session_dir, f'{i}.agi')) for i in ids),
                 os.path.getmtime(session_dir))
    cache = os.path.join(session_dir,
        f'.torchcache-look2-v12-b{args.blur}-e{args.eye_div}-f{args.face_mul}-s{args.size}-r{args.cam_rel_max}.npz')
    if os.path.exists(cache) and os.path.getmtime(cache) >= newest:
        z = np.load(cache)
        return ([z[k] for k in ('tl', 'tr', 'tf', 'tg', 'rf')],
                z['laux'], z['ly'], z['lfid'], z['tgl'], z['rfl'])
    print(f'loading {len(ids)} samples ...')
    from concurrent.futures import ProcessPoolExecutor
    with ProcessPoolExecutor() as ex:
        rows = [s for group in ex.map(load_one, [(session_dir, i) for i in ids],
                                      chunksize=16) for s in group]
    tl   = [r2[0] for r2 in rows]
    tr   = [r2[1] for r2 in rows]
    tf_  = [r2[2] for r2 in rows]
    tg   = [r2[3] for r2 in rows]
    rf   = [r2[4] for r2 in rows]
    laux = [r2[5] for r2 in rows]
    ly   = [r2[6] for r2 in rows]
    lfid = [r2[7] for r2 in rows]
    tgl  = [r2[8] for r2 in rows]
    rfl  = [r2[9] for r2 in rows]
    r = ([np.array(x, np.float32) for x in (tl, tr, tf_, tg, rf)],
         np.array(laux, np.float32), np.array(ly, np.float32),
         np.array(lfid), np.array(tgl, np.float32), np.array(rfl, np.float32))
    np.savez_compressed(cache, tl=r[0][0], tr=r[0][1], tf=r[0][2],
                        tg=r[0][3], rf=r[0][4],
                        laux=r[1], ly=r[2], lfid=r[3], tgl=r[4], rfl=r[5])
    return r


NAUX = 24

def conv_bn(ci, co, k=3, s=1):
    return nn.Sequential(nn.Conv2d(ci, co, k, s, k // 2, bias=False),
                         nn.BatchNorm2d(co), nn.ReLU())


class Enc(nn.Module):
    # crop encoder: BN conv trunk at full resolution -> k soft-argmax
    # sub-pixel (x,y) coordinates (a 1px iris shift moves them directly)
    # + 2x2-pooled appearance features. no Flatten->Linear on full-res
    # maps: that flat layer was the memorization surface; the 2x2 pool
    # keeps coarse layout and WHAT is seen, the coordinate path keeps
    # exact WHERE.
    def __init__(self, side, k=8, w=64):
        super().__init__()
        self.k = k
        self.trunk = nn.Sequential(
            conv_bn(1, 32, 5), conv_bn(32, w), conv_bn(w, w))
        self.heat = nn.Conv2d(w, k, 1)
        self.temp = nn.Parameter(torch.tensor(8.0))
        self.register_buffer('lin', torch.linspace(0.0, 1.0, side))
        self.app = nn.Sequential(
            conv_bn(w, w, s=2), conv_bn(w, 2 * w, s=2), conv_bn(2 * w, 2 * w, s=2),
            nn.AvgPool2d(max(1, side // 16)), nn.Flatten())
        self.fe = 2 * k + 2 * w * 4            # coords + pooled appearance

    def forward(self, x):
        t = self.trunk(x)                      # full resolution, no pooling
        hm = self.heat(t)
        B, k, S, _ = hm.shape
        p = torch.softmax(hm.reshape(B, k, -1) * self.temp, -1).reshape(B, k, S, S)
        xs = (p.sum(2) * self.lin).sum(-1)     # B,k sub-pixel x
        ys = (p.sum(3) * self.lin).sum(-1)     # B,k sub-pixel y
        return torch.cat([xs, ys, self.app(t)], 1)   # B, self.fe


class LookNet(nn.Module):
    # PAIRWISE and STAGED: both cameras enter one forward — screen
    # direction needs the pair, one view can't triangulate. aux =
    # both cams' image-plane floats + the offset stage's v3 (the eye
    # coordinate relative to the screen center, meters). the head
    # vector is inferred from the two FACE crops + aux first; the
    # gaze stage then consumes that inference, locked. aux keeps its
    # own unshared path so it never drowns in conv features
    def __init__(self, naux):
        super().__init__()
        self.naux = naux
        self.eye = Enc(args.size)
        self.face_enc = Enc(args.size)
        fe = self.face_enc.fe
        self.head_aux = nn.Sequential(
            nn.Linear(naux, 128), nn.ReLU(),
            nn.Linear(128, 128), nn.ReLU(), nn.Linear(128, 2))
        himg = nn.Linear(128, 2)
        nn.init.zeros_(himg.weight)
        nn.init.zeros_(himg.bias)
        self.head_img = nn.Sequential(nn.Linear(fe * 2, 128), nn.ReLU(), himg)
        self.delta = nn.Sequential(
            nn.Linear(fe * 6 + naux + 2, 128), nn.ReLU(), nn.Linear(128, 2))

    def head_params(self):
        return (list(self.face_enc.parameters()) +
                list(self.head_aux.parameters()) +
                list(self.head_img.parameters()))

    def gaze_params(self):
        return list(self.eye.parameters()) + list(self.delta.parameters())

    def forward(self, l0, r0, f0, l1, r1, f1, a):
        fc = torch.cat([self.face_enc(f0), self.face_enc(f1)], 1)
        head = self.head_aux(a) + self.head_img(fc)
        # gaze = anchored head + delta, with head DETACHED in the sum:
        # otherwise the (pixel-limited) gaze loss backprops through the
        # sum and drags head off its own inference
        d = self.delta(torch.cat(
            [self.eye(l0), self.eye(r0), self.eye(l1), self.eye(r1),
             fc.detach(), a, head.detach()], 1))
        return torch.cat([head, head.detach() + d], 1)


class OffsetNet(nn.Module):
    # ONE result from BOTH cameras together: the eye coordinate (the
    # oc-ic midpoint per eye, between both eyes) relative to the
    # SCREEN CENTER, meters, person frame. both face crops carry the
    # scale, both aux vectors carry where each camera saw the landmarks
    def __init__(self, naux):
        super().__init__()
        self.face_enc = Enc(args.size)
        fe = self.face_enc.fe
        self.out = nn.Sequential(
            nn.Linear(fe * 2 + naux * 2, 128), nn.ReLU(),
            nn.Linear(128, 128), nn.ReLU(), nn.Linear(128, 3))

    def forward(self, f0, f1, a):
        return self.out(torch.cat(
            [self.face_enc(f0), self.face_enc(f1), a], 1))


def nchw(x):
    return torch.from_numpy(x).permute(0, 3, 1, 2).contiguous()


def fit(m, xtr, ytr, xev, yev, cols, metric, out_name, staged=False, bench=None):
    dev = 'cuda' if torch.cuda.is_available() else 'cpu'
    m = m.to(dev)
    opt = torch.optim.AdamW(m.parameters(), lr=args.lr, weight_decay=args.wd)
    multi = isinstance(xtr, list)
    # staged: phase 1 trains the face->head vector alone (eyes untouched),
    # phase 2 locks everything facial and trains eyes+delta on gaze only
    half = (args.epochs + 1) // 2 if staged else 0
    if staged:
        opt = torch.optim.AdamW(m.head_params(), lr=args.lr, weight_decay=args.wd)
    if multi:
        xtr_t = [nchw(x).to(dev) for x in xtr[:-1]] + [torch.from_numpy(xtr[-1]).to(dev)]
        xev_t = [nchw(x).to(dev) for x in xev[:-1]] + [torch.from_numpy(xev[-1]).to(dev)]
    else:
        xtr_t = nchw(xtr).to(dev)
        xev_t = nchw(xev).to(dev)
    ytr_t = torch.from_numpy(ytr).to(dev)
    yev_t = torch.from_numpy(yev).to(dev)
    n = len(ytr)
    best, best_state, best_ep = None, None, 0
    # an epoch is a fixed amount of WORK, not one pass: small sets draw
    # with replacement up to `draw` samples so 30 epochs actually train
    draw = max(n, args.draw)
    # EASY eval subset: head within 25% of screen center and gaze within
    # 0.25 of the head's own point — the operating region that matters
    easy = None
    if args.easy_eval and yev.shape[1] == 4:
        easy = ((np.abs(yev[:, 0]) <= 0.25) & (np.abs(yev[:, 1]) <= 0.25) &
                (np.abs(yev[:, 2] - yev[:, 0]) <= 0.25) &
                (np.abs(yev[:, 3] - yev[:, 1]) <= 0.25))
        print(f'easy subset: {int(easy.sum())}/{len(yev)} eval rows')
        if easy.sum() == 0:
            easy = None
    print(f'epoch = {draw} draws ({max(1, draw // args.batch)} steps)')
    print('epoch      train        eval         ' + ''.join(f'{c:<13}' for c in cols)
          + ('' if easy is None else ''.join(f'ez.{c:<10}' for c in cols))
          + ('' if bench is None else 'bn.gx        bn.gy        '))
    for epoch in range(args.epochs):
        if staged and epoch == half:
            print(f'-- phase 2: face locked, training eyes+delta on gaze --')
            opt = torch.optim.AdamW(m.gaze_params(), lr=args.lr, weight_decay=args.wd)
        m.train()
        order = torch.randint(0, n, (draw,), device=dev)
        tot = 0.0
        for b in range(0, draw, args.batch):
            idx = order[b:b + args.batch]
            if multi:
                ins = [x[idx] for x in xtr_t]
                if args.aux_noise > 0:
                    ins[-1] = ins[-1] + torch.randn_like(ins[-1]) * args.aux_noise
                if args.px_noise > 0:
                    # one gain per sample across its crops (sensor exposure),
                    # then independent per-pixel noise — train only
                    g = torch.empty(len(idx), 1, 1, 1, device=dev).uniform_(0.92, 1.08)
                    ins[:-1] = [x * g + torch.randn_like(x) * args.px_noise
                                for x in ins[:-1]]
                p = m(*ins)
            else:
                x = xtr_t[idx]
                if args.px_noise > 0:
                    g = torch.empty(len(idx), 1, 1, 1, device=dev).uniform_(0.92, 1.08)
                    x = x * g + torch.randn_like(x) * args.px_noise
                p = m(x)
            if staged:
                sl = slice(0, 2) if epoch < half else slice(2, 4)
                loss = ((p[:, sl] - ytr_t[idx][:, sl]) ** 2).mean()
            else:
                loss = ((p - ytr_t[idx]) ** 2).mean()
            opt.zero_grad()
            loss.backward()
            opt.step()
            tot += float(loss.detach()) * len(idx)
        m.eval()
        with torch.no_grad():
            p = m(*xev_t) if multi else m(xev_t)
            eval_loss = float(((p - yev_t) ** 2).mean())
            aerr = (p - yev_t).abs().cpu().numpy()
            errs = aerr.mean(0)
        ez = '' if easy is None else ''.join(f'{e:<13.6g}' for e in aerr[easy].mean(0))
        bn = ''
        if bench is not None:
            be = bench_gaze(bench[0], bench[1], bench[2], bench[3], m, dev)
            if be is not None:
                bn = f'{be[0]:<13.6g}{be[1]:<13.6g}'
        print(f'{epoch + 1:>2}/{args.epochs:<7}'
              f'{tot / draw:<13.6g}{eval_loss:<13.6g}'
              + ''.join(f'{e:<13.6g}' for e in errs) + ez + bn)
        mval = float(metric(errs))
        # staged: gaze columns are untrained noise during phase 1
        if (not staged or epoch >= half) and (best is None or mval < best):
            best, best_ep = mval, epoch + 1
            best_state = {k: v.clone() for k, v in m.state_dict().items()}
    if best_state is not None:
        out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models')
        os.makedirs(out, exist_ok=True)
        torch.save(best_state, os.path.join(out, out_name))
        print(f'best epoch {best_ep} (err {best:.6g}) -> {out}/{out_name}')


class StageNet(nn.Module):
    # target/refine: an eye crop of varying size -> soft-argmax points
    # (k=1: eye center; k=2: center + outer corner) + log size correction
    def __init__(self, k=1):
        super().__init__()
        S = args.size
        self.trunk = nn.Sequential(
            nn.Conv2d(1, 8, 5, padding=2), nn.ReLU(), nn.MaxPool2d(2),
            nn.Conv2d(8, 16, 3, padding=1), nn.ReLU(),
            nn.Conv2d(16, 16, 3, padding=1), nn.ReLU())
        self.heat = nn.Conv2d(16, k, 1)
        self.temp = nn.Parameter(torch.tensor(8.0))
        self.register_buffer('lin', torch.linspace(-0.5, 0.5, S // 2))
        self.app = nn.Sequential(
            nn.MaxPool2d(2),
            nn.Conv2d(16, 16, 3, padding=1), nn.ReLU(), nn.MaxPool2d(2),
            nn.Flatten(),
            nn.Linear(16 * (S // 8) ** 2, 32), nn.ReLU(),
            nn.Linear(32, 1))                   # log size correction
    def forward(self, x):
        t = self.trunk(x)
        hm = self.heat(t)
        B, k, S, _ = hm.shape
        p = torch.softmax(hm.reshape(B, k, -1) * self.temp, -1).reshape(B, k, S, S)
        xs = (p.sum(2) * self.lin).sum(-1)
        ys = (p.sum(3) * self.lin).sum(-1)
        ds = self.app(t)
        pts = torch.stack([xs, ys], -1).reshape(B, k * 2)
        return torch.cat([pts, ds], 1)


def train_stage(crops, lab, lfid, name):
    # every framing pools into one net: right crops arrive pre-mirrored.
    # label width per framing sets the head count (3 = center+size,
    # 5 = center+outer corner+size)
    nc = crops.shape[-1]
    lw = lab.shape[1] // nc
    x = np.concatenate([crops[..., i:i + 1] for i in range(nc)])
    y = np.concatenate([lab[:, i * lw:(i + 1) * lw] for i in range(nc)]).astype(np.float32)
    fid2 = np.concatenate([lfid] * nc)
    evm = np.array([int(hashlib.md5(f'{args.seed}_{f}'.encode()).hexdigest(), 16) % 10 == 0
                    for f in fid2])
    ev = np.where(evm)[0]
    tr = np.where(~evm)[0]
    cols = ['x', 'y', 'ds'] if lw == 3 else ['x', 'y', 'ocx', 'ocy', 'ds']
    print(f'{name}: {len(tr)} train / {len(ev)} eval')
    fit(StageNet(k=(lw - 1) // 2), x[tr], y[tr], x[ev], y[ev],
        cols,
        lambda errs: errs[:2].mean(), f'{name}2-torch.pt')


def load_benchmark(session_dir):
    # real spiral-recorded pairs: {i}-top/bot.png + look label per .agi
    bd = os.path.join(session_dir, 'benchmark')
    if not os.path.isdir(bd):
        return []
    rows = []
    for f in sorted(os.listdir(bd)):
        if not f.endswith('.agi'):
            continue
        text = open(os.path.join(bd, f)).read()
        look = read_pair(text, 'look')
        if look[0] < -900:
            continue
        fid = f[:-4]
        from PIL import Image
        # pairwise look needs BOTH cameras of a capture
        pt = os.path.join(bd, f'{fid}-top.png')
        pb2 = os.path.join(bd, f'{fid}-bot.png')
        if not (os.path.exists(pt) and os.path.exists(pb2)):
            continue
        gt = np.asarray(Image.open(pt).convert('L'), np.float32) / 255.0
        gb = np.asarray(Image.open(pb2).convert('L'), np.float32) / 255.0
        rows.append((gt, gb, look[0], look[1]))
    return rows


def bench_cam(gray, tnet, rnet, dev):
    # one camera's localization: seat prior -> target twice -> refine.
    # returns the look crops + the 18 image-plane aux floats, or None
    S = args.size
    H, W = gray.shape
    eyes = [[0.58 * W, 0.45 * H, 0.07 * W],
            [0.42 * W, 0.45 * H, 0.07 * W]]
    pts = []
    for e, (ex, ey, es) in enumerate(eyes):
        mir = (e == 1)
        for _ in range(2):
            c, x0, y0, s0 = crop_gray(gray, ex, ey, es * 2.5, S)
            cc = c[:, ::-1] if mir else c
            o = tnet(torch.from_numpy(np.ascontiguousarray(
                cc.reshape(1, S, S, 1))).permute(0, 3, 1, 2).to(dev))[0].cpu().numpy()
            dx = -o[0] if mir else o[0]
            ex = x0 + s0 / 2 + dx * s0
            ey = y0 + s0 / 2 + o[1] * s0
            es = float(np.clip(np.exp(o[2]) * s0, 0.02 * W, 0.35 * W))
        c, x0, y0, s0 = crop_gray(gray, ex, ey, es, S)
        cc = c[:, ::-1] if mir else c
        o = rnet(torch.from_numpy(np.ascontiguousarray(
            cc.reshape(1, S, S, 1))).permute(0, 3, 1, 2).to(dev))[0].cpu().numpy()
        dx = -o[0] if mir else o[0]
        ox = -o[2] if mir else o[2]
        cx0, cy0 = x0 + s0 / 2, y0 + s0 / 2
        ex, ey = cx0 + dx * s0, cy0 + o[1] * s0
        pts.append((ex, ey, cx0 + ox * s0, cy0 + o[3] * s0))
    (lx, lyv, olx, oly), (rx, ryv, orx, ory) = pts
    sc = float(np.hypot(lx - rx, lyv - ryv)) / W
    if sc < 0.02:
        return None
    side = sc * W / args.eye_div
    lc, _, _, _ = crop_gray(gray, lx, lyv, side, S)
    rc, _, _, _ = crop_gray(gray, rx, ryv, side, S)
    fx, fy = (lx + rx) / 2, (lyv + ryv) / 2
    fc, _, _, _ = crop_gray(gray, fx, fy, sc * W * args.face_mul, S)
    aux = [lx / W - 0.5, lyv / H - 0.5, rx / W - 0.5, ryv / H - 0.5,
           olx / W - 0.5, oly / H - 0.5, orx / W - 0.5, ory / H - 0.5,
           sc,
           lx / W - 0.5, lyv / H - 0.5, side / W,
           rx / W - 0.5, ryv / H - 0.5, side / W,
           fx / W - 0.5, fy / H - 0.5, sc * args.face_mul]
    return lc, rc, fc, aux


def bench_gaze(rows, tnet, rnet, onet, lnet, dev):
    # the FULL cascade on real frames: each camera localizes alone,
    # then offset reads both, then PAIRWISE look reads both + offset
    S = args.size
    errs = []
    t = lambda x: torch.from_numpy(np.ascontiguousarray(
        x.reshape(1, S, S, 1))).permute(0, 3, 1, 2).to(dev)
    with torch.no_grad():
        for gtop, gbot, lu, lv in rows:
            c0 = bench_cam(gtop, tnet, rnet, dev)
            c1 = bench_cam(gbot, tnet, rnet, dev)
            if c0 is None or c1 is None:
                continue
            a36 = torch.from_numpy(np.array([c0[3] + c1[3]], np.float32)).to(dev)
            off = onet(t(c0[2]), t(c1[2]), a36)[0].cpu().numpy()
            aux = np.array([c0[3] + c1[3] + list(off)], np.float32)
            p = lnet(t(c0[0]), t(c0[1]), t(c0[2]),
                     t(c1[0]), t(c1[1]), t(c1[2]),
                     torch.from_numpy(aux).to(dev))[0].cpu().numpy()
            errs.append((abs(p[2] - lu), abs(p[3] - lv)))
    if not errs:
        return None
    return np.mean(np.array(errs), 0)


def export_ts():
    # trace the trained cascade for the app's libtorch runtime
    here = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models')
    S = args.size
    jobs = [('target2-torch.pt', lambda: StageNet(k=1),
             (torch.zeros(1, 1, S, S),), 'target2.ptc'),
            ('refine2-torch.pt', lambda: StageNet(k=2),
             (torch.zeros(1, 1, S, S),), 'refine2.ptc'),
            ('look2-torch.pt', lambda: LookNet(39),
             (torch.zeros(1, 1, S, S), torch.zeros(1, 1, S, S),
              torch.zeros(1, 1, S, S), torch.zeros(1, 1, S, S),
              torch.zeros(1, 1, S, S), torch.zeros(1, 1, S, S),
              torch.zeros(1, 39)), 'look2.ptc'),
            ('offset2-torch.pt', lambda: OffsetNet(18),
             (torch.zeros(1, 1, S, S), torch.zeros(1, 1, S, S),
              torch.zeros(1, 36)), 'offset2.ptc')]
    for src, mk, ex, dst in jobs:
        p = os.path.join(here, src)
        if not os.path.exists(p):
            print(f'export_ts: {src} missing, skipped')
            continue
        m = mk()
        m.load_state_dict(torch.load(p, map_location='cpu'))
        m.eval()
        ts = torch.jit.trace(m, ex)
        ts.save(os.path.join(here, dst))
        print(f'export_ts: {dst}')


def main():
    if args.seed:
        np.random.seed(args.seed)
        torch.manual_seed(args.seed)
    if args.export_ts:
        export_ts()
        return
    # --session takes a comma list: sets concatenate, fids stay unique
    # per session so the eval holdout never pairs across sets
    names = [s.strip() for s in args.session.split(',') if s.strip()]
    parts = [load_session(f'/src/hyperspace-sessions/{s}') for s in names]
    limgs = [np.concatenate([p[0][k] for p in parts]) for k in range(5)]
    laux = np.concatenate([p[1] for p in parts])
    ly   = np.concatenate([p[2] for p in parts])
    lfid = np.concatenate([p[3] + 1000000 * i for i, p in enumerate(parts)])
    tglab = np.concatenate([p[4] for p in parts])
    rflab = np.concatenate([p[5] for p in parts])
    if args.process == 'target':
        train_stage(limgs[3], tglab, lfid, 'target')
        return
    if args.process == 'refine':
        train_stage(limgs[4], rflab, lfid, 'refine')
        return
    if args.process == 'offset':
        # pair the per-camera rows back up by fid: one row per PAIR
        # (top face crop, bottom face crop, both aux) -> one v3, the
        # eye coordinate relative to the screen center in meters
        naux = 18
        by = {}
        for i, f in enumerate(lfid):
            by.setdefault(int(f), []).append(i)
        f0, f1, ax, oy, ofid = [], [], [], [], []
        for f, idxs in sorted(by.items()):
            if len(idxs) != 2:
                continue
            a, b = idxs
            if ly[a][4] < -900:
                continue
            f0.append(limgs[2][a])
            f1.append(limgs[2][b])
            ax.append(np.concatenate([laux[a][:naux], laux[b][:naux]]))
            oy.append(ly[a][4:7])
            ofid.append(f)
        if not oy:
            raise SystemExit('offset: no pairs carry eye_off — regenerate the session')
        f0 = np.array(f0, np.float32)
        f1 = np.array(f1, np.float32)
        ax = np.array(ax, np.float32)
        oy = np.array(oy, np.float32)
        evm2 = np.array([int(hashlib.md5(f'{args.seed}_{f}'.encode()).hexdigest(), 16) % 10 == 0
                         for f in ofid])
        ev2 = np.where(evm2)[0]
        tr2 = np.where(~evm2)[0]
        print(f'offset: {len(tr2)} train / {len(ev2)} eval pairs')
        fit(OffsetNet(naux), [f0[tr2], f1[tr2], ax[tr2]], oy[tr2],
            [f0[ev2], f1[ev2], ax[ev2]], oy[ev2],
            ['off.x', 'off.y', 'off.z'],
            lambda errs: errs.mean(), 'offset2-torch.pt')
        return
    if args.stats:
        n = len(ly)
        print(f'== dataset: {args.session} ({n} samples) ==')
        for name, pts in (('gaze', ly[:, 2:4]), ('head', ly[:, :2])):
            print(f'  {name}: x {pts[:,0].min():+.3f}..{pts[:,0].max():+.3f}'
                  f'  y {pts[:,1].min():+.3f}..{pts[:,1].max():+.3f}')
        print(f'  scale: mean {laux[:,8].mean():.3f} span {laux[:,8].min():.3f}..{laux[:,8].max():.3f}')
        return
    if args.preview:
        # previews are ALWAYS written as {i}-preview.png beside each sample
        # during cache build; this flag just forces that rebuild
        for s in names:
            d = f'/src/hyperspace-sessions/{s}'
            for f in os.listdir(d):
                if f.startswith('.torchcache-'):
                    os.remove(os.path.join(d, f))
            load_session(d)
        print('previews rebuilt beside samples ({i}-preview.png)')
        return
    # PAIRWISE look: both cameras in one forward. rows pair by fid;
    # aux = both cams' image-plane floats + the offset stage's v3,
    # teacher-forced from the eye_off label (runtime feeds offset2's
    # prediction — the same contract every other aux slot follows)
    if args.zero_face:
        limgs[2] = np.zeros_like(limgs[2])
    naux1 = 24 if args.ray_aux else 18
    by2 = {}
    for i, f in enumerate(lfid):
        by2.setdefault(int(f), []).append(i)
    pa, pb, pax, pyl, pfid = [], [], [], [], []
    for f, idxs in sorted(by2.items()):
        if len(idxs) != 2:
            continue
        a, b = idxs
        if ly[a][4] < -900:
            continue
        pa.append(a)
        pb.append(b)
        pax.append(np.concatenate([laux[a][:naux1], laux[b][:naux1], ly[a][4:7]]))
        pyl.append(ly[a][:4])
        pfid.append(f)
    if not pa:
        raise SystemExit('look: no pairs carry eye_off — regenerate the session')
    pa = np.array(pa)
    pb = np.array(pb)
    pax = np.array(pax, np.float32)
    pyl = np.array(pyl, np.float32)
    naux = naux1 * 2 + 3
    # deterministic 10% eval holdout, by PAIR
    evm = np.array([int(hashlib.md5(f'{args.seed}_{f}'.encode()).hexdigest(), 16) % 10 == 0
                    for f in pfid])
    ev = np.where(evm)[0]
    tr = np.where(~evm)[0]
    print(f'look: {len(tr)} train / {len(ev)} eval pairs')
    atr, aev = pax[tr], pax[ev]
    if args.zero_aux:
        atr = np.zeros_like(atr)
        aev = np.zeros_like(aev)
    atr = atr[:, :naux].copy()
    aev = aev[:, :naux].copy()
    # real-world test column: spiral-recorded benchmark pairs run the
    # FULL cascade (frozen target+refine localize, this look net reads)
    bench = None
    names0 = [s.strip() for s in args.session.split(',') if s.strip()]
    brows = []
    for s0 in names0:
        brows += load_benchmark(f'/src/hyperspace-sessions/{s0}')
    if brows:
        here = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models')
        tp0 = os.path.join(here, 'target2-torch.pt')
        rp0 = os.path.join(here, 'refine2-torch.pt')
        op0 = os.path.join(here, 'offset2-torch.pt')
        if os.path.exists(tp0) and os.path.exists(rp0) and os.path.exists(op0):
            dev0 = 'cuda' if torch.cuda.is_available() else 'cpu'
            tnet = StageNet(k=1).to(dev0)
            tnet.load_state_dict(torch.load(tp0, map_location=dev0))
            tnet.eval()
            rnet = StageNet(k=2).to(dev0)
            rnet.load_state_dict(torch.load(rp0, map_location=dev0))
            rnet.eval()
            onet = OffsetNet(18).to(dev0)
            onet.load_state_dict(torch.load(op0, map_location=dev0))
            onet.eval()
            bench = (brows, tnet, rnet, onet)
            print(f'benchmark: {len(brows)} real pairs (full-cascade test)')
        else:
            print('benchmark: pairs found but stage models missing — train target/refine/offset first')
    fit(LookNet(naux),
        [limgs[0][pa[tr]], limgs[1][pa[tr]], limgs[2][pa[tr]],
         limgs[0][pb[tr]], limgs[1][pb[tr]], limgs[2][pb[tr]], atr], pyl[tr],
        [limgs[0][pa[ev]], limgs[1][pa[ev]], limgs[2][pa[ev]],
         limgs[0][pb[ev]], limgs[1][pb[ev]], limgs[2][pb[ev]], aev], pyl[ev],
        ['head.x', 'head.y', 'gaze.x', 'gaze.y'],
        lambda errs: errs[2:].mean(), 'look2-torch.pt', staged=True, bench=bench)


if __name__ == '__main__':
    main()
