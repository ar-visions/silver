#!/usr/bin/env python3
# torch trainer for hyperspace synthetic sessions. samples are
# {i}.agi + {i}-face{c}.png; crops are cut at the exact labels.
# every network input is 16x16.
#
# label conventions (zero-centered, -0.5 left/top .. +0.5 right/bottom):
#   look / head        screen plots
#   pupil_left/right   IRIS centers — THE left/right eye labels
#   face_left/right    resting socket centers (sigma scale only)
#   face_*_oc          outer eye corners in the face image
#   nose_base          under-nose point (-999 = absent, older sessions)
#   head_center        screen center -> face, rig-space meters
#   cam{N}             camera x y z tilt fov, meters from screen center
# face scale = |left_oc - right_oc| (2D, image-width fraction)
# pixel position in the saved image = (plot + 0.5) * dims
import argparse, os, re, hashlib
import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument('--session',  default='default')
    p.add_argument('--epochs',   type=int,   default=100)
    p.add_argument('--lr',       type=float, default=0.00002)
    p.add_argument('--batch',    type=int,   default=64)
    p.add_argument('--draw',     type=int,   default=20000)  # samples per epoch
    p.add_argument('--size',     type=int,   default=16)     # every net input side
    p.add_argument('--seed',     type=int,   default=1234)
    p.add_argument('--eye_div',  type=float, default=3.0)    # eye side = face side/eye_div
    p.add_argument('--face_mul', type=float, default=1.0)    # face side = scale*face_mul
    p.add_argument('--ctx_mul',  type=float, default=3.0)    # target context = scale*ctx_mul
    p.add_argument('--ctx_size', type=int,   default=64)     # cached context crop px
    # find | target | look, or all three in that order
    p.add_argument('--process',  default='all',
                   choices=['all', 'find', 'target', 'look'])
    p.add_argument('--aux_noise', type=float, default=0.0)
    # sensor model: per-sample gain + per-pixel gaussian, train only
    p.add_argument('--px_noise', type=float, default=0.008)
    # landmark scramble on the look crops: noise-capture patches
    # fuzzed in with perlin alpha, blur, smear — everywhere except
    # annotated points (eye centers, eye sides). prob per overlay
    p.add_argument('--scramble', type=float, default=0.7)
    p.add_argument('--tex_n',    type=int,   default=4000)   # donor patch pool
    # optics blur radius on the frame at load, fraction of side
    p.add_argument('--blur',     type=float, default=0.0022)
    # pupil heat-map loss: DEAD since crops centered on irises —
    # the target became a constant center. 0 keeps train loss pure
    # mse, comparable with eval
    p.add_argument('--pupil_w',  type=float, default=0.0)
    p.add_argument('--holdout',  type=float, default=10.0)   # eval percent, id-seeded
    # synthetic frames pass through the same sensor as reality:
    # downsampled to camera width before any crop is cut
    p.add_argument('--sensor_w', type=int,   default=340)
    # real validation crops blend into matched training samples
    # (look and head plots agree within 0.15): apply probability
    p.add_argument('--real_blend', type=float, default=0.5)
    p.add_argument('--stats',    action='store_true')
    p.add_argument('--preview',  action='store_true')
    return p.parse_args()

args = parse_args()
dev = torch.device('cuda' if torch.cuda.is_available() else 'cpu')

# standard crop pads — training, eval, validation all use these
EYE_PAD  = 1.25
FACE_PAD = 1.15


NUM = r'-?[\d.]+(?:[eE][+-]?\d+)?'

def read_pair(text, key):
    m = re.search(rf'{re.escape(key)}:\s*({NUM})(?:[ \t]+({NUM}))?', text)
    if not m:
        return (-999.0, -999.0)
    return (float(m.group(1)), float(m.group(2)) if m.group(2) else 0.0)


def read3(text, key):
    m = re.search(rf'{re.escape(key)}:\s*(-?[\d.eE+-]+)\s+(-?[\d.eE+-]+)\s+(-?[\d.eE+-]+)', text)
    return [float(m.group(i)) for i in (1, 2, 3)] if m else [0.0, 0.0, 0.0]


def crop_gray(gray, cx, cy, side, out):
    # square crop centered at (cx, cy); out-of-frame padded
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
    arr = np.asarray(c.resize((out, out), Image.BOX), np.float32)[None] / 255.0
    return arr, x0, y0, s


def load_one(job):
    session_dir, fid = job
    text = open(os.path.join(session_dir, f'{fid}.agi')).read()
    cams = int(read_pair(text, 'cameras')[0])
    out = []
    for c in range(max(cams, 0)):
        s = load_cam(session_dir, fid, text, str(c))
        if s:
            out.append(s)
    return out


def load_cam(session_dir, fid, text, sfx):
    from PIL import Image, ImageDraw
    head = read_pair(text, 'head')
    look = read_pair(text, 'look')
    el   = read_pair(text, f'face_left{sfx}')
    er   = read_pair(text, f'face_right{sfx}')
    ocl  = read_pair(text, f'face_left_oc{sfx}')
    ocr  = read_pair(text, f'face_right_oc{sfx}')
    nb   = read_pair(text, f'nose_base{sfx}')
    # face scale = outer-corner span; eye-center span keeps sigma
    sc  = float(np.hypot(ocl[0] - ocr[0], ocl[1] - ocr[1])) \
        if ocl[0] > -900 and ocr[0] > -900 else 0.0
    scc = float(np.hypot(el[0] - er[0], el[1] - er[1]))
    # distance is from the CAMERA: |head_center - cam position|
    hc3 = read3(text, 'head_center')
    cam_dist = read_pair(text, 'head_dist')[0]
    if re.search(rf'cam{sfx}:', text):
        cd3 = np.array(hc3) - np.array(read3(text, f'cam{sfx}'))
        cam_dist = float(np.sqrt((cd3 * cd3).sum()))
    geo = [hc3[0], hc3[1], hc3[2], cam_dist]
    pl   = read_pair(text, f'pupil_left{sfx}')
    pr   = read_pair(text, f'pupil_right{sfx}')
    prad = read_pair(text, 'pupil_rad')[0]
    clean = 1.0 if re.search(r'clean:\s*1', text) else 0.0
    if look[0] < -900 or el[0] < -900 or pl[0] < -900 or sc <= 0:
        return None
    img = Image.open(os.path.join(session_dir, f'{fid}-face{sfx}.png')).convert('L')
    if args.sensor_w > 0 and img.width > args.sensor_w:
        img = img.resize((args.sensor_w, args.sensor_w), Image.BOX)
    if args.blur > 0:
        from PIL import ImageFilter
        brng = np.random.RandomState(fid * 31 + int(sfx))
        brad = (args.blur / 2 + brng.rand() * args.blur / 2) * img.width
        img = img.filter(ImageFilter.GaussianBlur(brad))
    gray = np.asarray(img, np.float32) / 255.0
    H, W = gray.shape
    # the eye labels ARE the irises: crops center on them
    lx, lyv = (pl[0] + 0.5) * W, (pl[1] + 0.5) * H
    rx, ryv = (pr[0] + 0.5) * W, (pr[1] + 0.5) * H
    # standard pads, every consumer: eyes +25%, face +15%
    fside = sc * W * args.face_mul * FACE_PAD
    side = sc * W * args.face_mul / args.eye_div * EYE_PAD
    lc, lx0, ly0, ls = crop_gray(gray, lx, lyv, side, args.size)
    rc, rx0, ry0, rs = crop_gray(gray, rx, ryv, side, args.size)
    # face crop centered on the eyes+nose centroid
    if nb[0] > -900:
        fx = (lx + rx + (nb[0] + 0.5) * W) / 3
        fy = (lyv + ryv + (nb[1] + 0.5) * H) / 3
    else:
        fx, fy = (lx + rx) / 2, (lyv + ryv) / 2
    fc, fx0, fy0, fs = crop_gray(gray, fx, fy, fside, args.size)
    # whole frame reduced: the find net's input
    wf = np.asarray(img.resize((args.size, args.size), Image.BOX),
                    np.float32)[None] / 255.0
    # context crop for the target net, eye-mid centered
    ccx, ccy = (lx + rx) / 2, (lyv + ryv) / 2
    cc, cx0, cy0, cs = crop_gray(gray, ccx, ccy, sc * W * args.ctx_mul,
                                 args.ctx_size)
    # target labels: eyes as 0..1 context fractions, scale as a
    # context-side fraction, then head_center xyz + camera distance
    def ctx_frac(p):
        return (((p[0] + 0.5) * W - cx0) / cs, ((p[1] + 0.5) * H - cy0) / cs)
    clu, clv = ctx_frac(pl)
    cru, crv = ctx_frac(pr)
    tmeta = [clu, clv, cru, crv, sc * W / cs] + geo
    def box(x0, y0, s):
        return [(x0 + s / 2) / W - 0.5, (y0 + s / 2) / H - 0.5, s / W]
    # aux is only what reality can supply: landmark plots, scale,
    # crop boxes. head_rot/face_off are label-only — NOT modelled
    aux = ([pl[0], pl[1], pr[0], pr[1],
            ocl[0], ocl[1], ocr[0], ocr[1], sc]
           + box(lx0, ly0, ls) + box(rx0, ry0, rs) + box(fx0, fy0, fs))
    y = [head[0], head[1], look[0], look[1]]
    # pupil labels as eye-crop fractions + sigma; sigma 0 gates off
    def in_crop(p, x0, y0, s):
        return ((p[0] + 0.5) * W - x0) / s, ((p[1] + 0.5) * H - y0) / s
    plu, plv = in_crop(pl, lx0, ly0, ls)
    pru, prv = in_crop(pr, rx0, ry0, rs)
    sig = prad * scc * W / ls if (prad > 0 and pl[0] > -900) else 0.0
    if not (0.0 < plu < 1.0 and 0.0 < plv < 1.0 and
            0.0 < pru < 1.0 and 0.0 < prv < 1.0):
        sig = 0.0
    pup = [plu, plv, pru, prv, sig, clean]
    # {i}-preview.png: the annotated frame validates every label
    pim = img.convert('RGB')
    dr = ImageDraw.Draw(pim)
    def cross(x, y2, col, r2=4):
        px, py = (x + 0.5) * W, (y2 + 0.5) * H
        dr.line([px - r2, py, px + r2, py], fill=col, width=2)
        dr.line([px, py - r2, px, py + r2], fill=col, width=2)
    cross(pl[0], pl[1], (0, 255, 0))
    cross(pr[0], pr[1], (0, 255, 0))
    if nb[0] > -900:
        cross(nb[0], nb[1], (255, 255, 0))
    dr.rectangle([lx0, ly0, lx0 + ls, ly0 + ls], outline=(255, 64, 64), width=2)
    dr.rectangle([rx0, ry0, rx0 + rs, ry0 + rs], outline=(64, 128, 255), width=2)
    dr.rectangle([fx0, fy0, fx0 + fs, fy0 + fs], outline=(255, 255, 255), width=2)
    pim = pim.resize((512, 512), Image.BOX)
    dr = ImageDraw.Draw(pim)
    dr.text((6, 6), f'head {head[0]:+.3f} {head[1]:+.3f}  '
                    f'gaze {look[0]:+.3f} {look[1]:+.3f}  scale {sc:.3f}  '
                    f'dist {cam_dist:.3f}',
            fill=(0, 255, 0))
    pim.save(os.path.join(session_dir, f'{fid}-preview{sfx}.png'))
    # the finished inputs (left | right | face), 8x nearest
    strip = np.concatenate([lc, rc, fc], axis=2)[0]
    simg = Image.fromarray((strip * 255).astype(np.uint8))
    simg = simg.resize((simg.width * 8, simg.height * 8), Image.NEAREST)
    simg.save(os.path.join(session_dir, f'{fid}-inputs{sfx}.png'))
    return lc, rc, fc, wf, aux, y, fid, pup, geo, cc, tmeta


def load_session(session_dir):
    ids = sorted(int(f[:-4]) for f in os.listdir(session_dir)
                 if f.endswith('.agi') and f[:-4].isdigit())
    if not ids:
        raise SystemExit(f'no .agi samples in {session_dir}')
    newest = max(max(os.path.getmtime(os.path.join(session_dir, f'{i}.agi')) for i in ids),
                 os.path.getmtime(session_dir))
    cache = os.path.join(session_dir,
        f'.cache-v8-b{args.blur}-e{args.eye_div}-f{args.face_mul}'
        f'-w{args.sensor_w}'
        f'-s{args.size}-c{args.ctx_mul}-x{args.ctx_size}.npz')
    keys = ('tl', 'tr', 'tf', 'tw', 'laux', 'ly', 'lfid', 'lpup',
            'lgeo', 'tcc', 'tmt')
    if os.path.exists(cache) and os.path.getmtime(cache) >= newest:
        z = np.load(cache)
        return [z[k] for k in keys]
    print(f'loading {len(ids)} samples ...')
    from concurrent.futures import ProcessPoolExecutor
    with ProcessPoolExecutor() as ex:
        rows = [s for group in ex.map(load_one, [(session_dir, i) for i in ids],
                                      chunksize=16) for s in group]
    cols = list(zip(*rows))
    r = [np.array(c, np.float32) if k != 'lfid' else np.array(c)
         for k, c in zip(keys, cols)]
    np.savez_compressed(cache, **dict(zip(keys, r)))
    return r


NAUX = 18


def conv_bn(ci, co, k=3, s=1):
    return nn.Sequential(
        nn.Conv2d(ci, co, k, stride=s, padding=k // 2, bias=False),
        nn.BatchNorm2d(co), nn.ReLU())


def norm_input(x):
    # per-sample zero mean / unit std: exposure and gain drop out,
    # so dark real captures and bright synthetic frames match
    m = x.mean(dim=(1, 2, 3), keepdim=True)
    s = x.std(dim=(1, 2, 3), keepdim=True).clamp(min=1e-4)
    return (x - m) / s


def soft_argmax(hm, temp, lin):
    # per-channel sub-pixel (x, y) from a softmax heat map
    B, k, S, _ = hm.shape
    p = torch.softmax(hm.reshape(B, k, S * S) * temp, 2).reshape(B, k, S, S)
    xs = (p.sum(2) * lin).sum(2)
    ys = (p.sum(3) * lin).sum(2)
    return p, xs, ys


class Enc(nn.Module):
    # crop encoder: full-res trunk -> k soft-argmax coordinates
    # (exact WHERE) + strided appearance features (coarse WHAT)
    def __init__(self, side, k=8, w=64):
        super().__init__()
        self.k = k
        self.trunk = nn.Sequential(conv_bn(1, 32, 5), conv_bn(32, w), conv_bn(w, w))
        self.heat = nn.Conv2d(w, k, 1)
        self.temp = nn.Parameter(torch.tensor(8.0))
        self.register_buffer('lin', torch.linspace(0.0, 1.0, side))
        self.app = nn.Sequential(
            conv_bn(w, w, s=2), conv_bn(w, 2 * w, s=2), conv_bn(2 * w, 2 * w, s=2),
            nn.Flatten())
        self.fe = 2 * k + 2 * w * (side // 8) ** 2

    def forward(self, x, want_hm=False):
        t = self.trunk(norm_input(x))
        hm = self.heat(t)
        _, xs, ys = soft_argmax(hm, self.temp, self.lin)
        out = torch.cat([xs, ys, self.app(t)], 1)
        return (out, hm) if want_hm else out


class LookNet(nn.Module):
    # aux is required geometry: crop boxes + head rotation anchor.
    # head from aux with a zero-init image correction; gaze =
    # detached head + delta so the gaze loss never drags head
    def __init__(self):
        super().__init__()
        self.eye = Enc(args.size)
        self.face_enc = Enc(args.size)
        fe = self.eye.fe
        self.head_aux = nn.Sequential(
            nn.Linear(NAUX, 128), nn.ReLU(),
            nn.Linear(128, 128), nn.ReLU(), nn.Linear(128, 2))
        hi_last = nn.Linear(128, 2)
        nn.init.zeros_(hi_last.weight)
        nn.init.zeros_(hi_last.bias)
        self.head_img = nn.Sequential(nn.Linear(fe, 128), nn.ReLU(), hi_last)
        self.delta = nn.Sequential(
            nn.Linear(3 * fe + NAUX + 2, 128), nn.ReLU(), nn.Linear(128, 2))
        # geometry head: head_center xyz + camera distance, aux-led
        # with a zero-init image correction like head
        gi_last = nn.Linear(128, 4)
        nn.init.zeros_(gi_last.weight)
        nn.init.zeros_(gi_last.bias)
        self.geo_aux = nn.Sequential(
            nn.Linear(NAUX, 128), nn.ReLU(),
            nn.Linear(128, 128), nn.ReLU(), nn.Linear(128, 4))
        self.geo_img = nn.Sequential(nn.Linear(fe, 128), nn.ReLU(), gi_last)

    def pupil_map_loss(self, hm, u, v, sig):
        # heat channel 0 must BE the pupil: gaussian-bump cross
        # entropy + penalty on mass outside 2.5 sigma
        B, _, S, _ = hm.shape
        p = torch.softmax(hm[:, 0].reshape(B, S * S) * self.eye.temp, 1)
        lin = torch.linspace(0.0, 1.0, S, device=hm.device)
        gx = lin.repeat(S)
        gy = lin.repeat_interleave(S)
        s2 = (sig.clamp(min=1e-3) ** 2).unsqueeze(1)
        d2 = ((gx[None] - u[:, None]) ** 2
            + (gy[None] - v[:, None]) ** 2) / (2.0 * s2)
        bump = torch.exp(-d2)
        bump = bump / (bump.sum(1, keepdim=True) + 1e-9)
        ce = -(bump * torch.log(p + 1e-9)).sum(1)
        outside = (p * (d2 > 3.125).float()).sum(1)
        valid = (sig > 0.0).float()
        return (valid * (ce + outside)).sum() / (valid.sum() + 1e-6)

    def forward(self, l, r, f, a, pl, pr):
        fc = self.face_enc(f)
        el, hml = self.eye(l, want_hm=True)
        er, hmr = self.eye(r, want_hm=True)
        head = self.head_aux(a) + self.head_img(fc)
        hsg = head.detach()
        d = self.delta(torch.cat([el, er, fc, a, hsg], 1))
        geo = self.geo_aux(a) + self.geo_img(fc)
        ploss = 0.0
        if self.training and args.pupil_w > 0:
            ploss = args.pupil_w * (
                self.pupil_map_loss(hml, pl[:, 0], pl[:, 1], pl[:, 2])
              + self.pupil_map_loss(hmr, pr[:, 0], pr[:, 1], pr[:, 2]))
        # [face uv, gaze uv, head_center xyz, camera distance]
        return torch.cat([head, hsg + d, geo], 1), ploss


class PointNet(nn.Module):
    # shared find/target shape: 2 supervised soft-argmax channels
    # (eye centers) + pooled head. find outputs scale only; target
    # adds head_center xyz + camera distance once locked on
    def __init__(self, lo, hi, pooled_out):
        super().__init__()
        S = args.size
        self.trunk = nn.Sequential(
            nn.Conv2d(1, 8, 5, padding=2), nn.ReLU(),
            nn.Conv2d(8, 16, 3, padding=1), nn.ReLU(),
            nn.Conv2d(16, 16, 3, padding=1), nn.ReLU())
        self.heat = nn.Conv2d(16, 2, 1)
        self.temp = nn.Parameter(torch.tensor(8.0))
        self.register_buffer('lin', torch.linspace(lo, hi, S))
        self.app = nn.Sequential(
            nn.MaxPool2d(2), nn.Conv2d(16, 16, 3, padding=1), nn.ReLU(),
            nn.MaxPool2d(2), nn.Conv2d(16, 16, 3, padding=1), nn.ReLU(),
            nn.Flatten(), nn.Linear(16 * (S // 4) ** 2, 32), nn.ReLU(),
            nn.Linear(32, pooled_out))

    def forward(self, x):
        t = self.trunk(norm_input(x))
        hm = self.heat(t)
        _, xs, ys = soft_argmax(hm, self.temp, self.lin)
        sc = self.app(t)
        return torch.cat([xs[:, :1], ys[:, :1], xs[:, 1:], ys[:, 1:], sc], 1)


_blur_bank = None

def blur_bank():
    # blur levels 0.22..4.4% of the side — the FIELD interpolates
    # between these per pixel
    global _blur_bank
    if _blur_bank is None:
        _blur_bank = []
        for f in np.linspace(0.0022, 0.044, 6):
            sig = f * args.size
            _blur_bank.append(gauss_kernel(sig, max(3, int(3 * sig) * 2 + 1)))
    return _blur_bank


def level_stats(arr):
    # (mean, std) population measured from the validation crops
    m = arr.mean(axis=(1, 2, 3))
    s = arr.std(axis=(1, 2, 3))
    return torch.tensor(np.stack([m, s], 1).astype(np.float32)).to(dev)


def renorm_levels(x, tstats, pick=None):
    # renormalize to the COLOR LEVELS of a real validation image —
    # runs BEFORE the noise overlay so donors blend at real levels
    if tstats is None:
        return x
    if pick is None:
        pick = torch.randint(0, tstats.shape[0], (x.shape[0],),
                             device=x.device)
    t = tstats[pick]
    m = x.mean(dim=(1, 2, 3), keepdim=True)
    s = x.std(dim=(1, 2, 3), keepdim=True).clamp(min=1e-4)
    return (x - m) / s * t[:, 1, None, None, None] + t[:, 0, None, None, None]


_pupil_keep = None

def pupil_keep():
    # center disc of an eye crop: the bright-pupil dot lives here
    # and is the strongest real gaze feature — blur must not eat it
    global _pupil_keep
    if _pupil_keep is None:
        S = args.size
        c = (S - 1) / 2
        yy, xx = torch.meshgrid(torch.arange(S, dtype=torch.float32),
                                torch.arange(S, dtype=torch.float32),
                                indexing='ij')
        m = ((xx - c) ** 2 + (yy - c) ** 2 <= (S * 0.22) ** 2).float()
        _pupil_keep = m[None, None].to(dev)
    return _pupil_keep


def sensor(x, keep=None):
    # a true blur FIELD: perlin picks the local blur strength per
    # PIXEL; `keep` regions retain most of their pre-blur detail
    bank = blur_bank()
    K = len(bank)
    x = x + torch.randn_like(x) * args.px_noise
    variants = []
    for kk in bank:
        p = kk.shape[-1] // 2
        variants.append(F.conv2d(F.pad(x, (p, p, p, p), mode='replicate'), kk))
    v = torch.stack(variants, 4)               # B,1,S,S,K
    B, _, S, _ = x.shape
    idx = perlin(B, S, x.device)[..., None] * (K - 1)
    lo = idx.floor().long().clamp(0, K - 1)
    hi = (lo + 1).clamp(max=K - 1)
    w = idx - lo.float()
    out = (v.gather(4, lo) * (1 - w) + v.gather(4, hi) * w)[..., 0]
    if keep is not None:
        out = out + keep * 0.75 * (x - out)
    return out


def loop(model, name, cols, metric, steps_data, eval_data, val_fn=None):
    model.to(dev)
    opt = torch.optim.Adam(model.parameters(), lr=args.lr)
    n, step_fn = steps_data
    draw = max(n, args.draw)
    steps = max(1, draw // args.batch)
    print(f'epoch = {draw} draws ({steps} steps)')
    vcols = []
    if val_fn:
        with torch.no_grad():
            vcols, _ = val_fn(model)
    print('epoch      train        eval         '
          + ''.join(f'{c:<13}' for c in cols + vcols))
    best, best_w, best_ep = None, None, 0
    for ep in range(args.epochs):
        model.train()
        tot = 0.0
        for _ in range(steps):
            loss = step_fn(model)
            opt.zero_grad()
            loss.backward()
            opt.step()
            tot += loss.detach().item()
        model.eval()
        with torch.no_grad():
            p, yev = eval_data(model)
            vvals = val_fn(model)[1] if val_fn else []
        eval_loss = float(((p - yev) ** 2).mean())
        errs = (p - yev).abs().mean(0).cpu().numpy()
        print(f'{ep + 1:>2}/{args.epochs:<7}'
              f'{tot / steps:<13.6g}{eval_loss:<13.6g}'
              + ''.join(f'{e:<13.6g}' for e in errs)
              + ''.join(f'{v:<13.6g}' for v in vvals))
        mval = float(metric(errs))
        if best is None or mval < best:
            best, best_ep = mval, ep + 1
            best_w = {k: v.detach().clone() for k, v in model.state_dict().items()}
    if best_w is not None:
        out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models')
        os.makedirs(out, exist_ok=True)
        model.load_state_dict(best_w)
        torch.save(model.state_dict(), os.path.join(out, f'{name}.pt'))
        print(f'best epoch {best_ep} (err {best:.6g}) -> {out}/{name}.pt')
        export_ts(model, name, out)


class LookWrap(nn.Module):
    # torchscript face: (l, r, f, aux) -> 8 outputs, no pupil args
    def __init__(self, m):
        super().__init__()
        self.m = m

    def forward(self, l, r, f, a):
        z = torch.zeros(l.shape[0], 3, device=l.device)
        out, _ = self.m(l, r, f, a, z, z)
        return out


def export_ts(model, name, out):
    # the app runs these live through the torchshim (.ptc)
    S = args.size
    m2 = model.to('cpu').eval()
    one = torch.zeros(1, 1, S, S)
    with torch.no_grad():
        if isinstance(m2, LookNet):
            w = LookWrap(m2)
            ts = torch.jit.trace(w, (one, one, one, torch.zeros(1, NAUX)))
        else:
            ts = torch.jit.trace(m2, one)
    p = os.path.join(out, f'{name}.ptc')
    ts.save(p)
    print(f'torchscript -> {p}')
    model.to(dev)


def split(lfid, lpup, label, clean_ok=True):
    # the sample id seeds an N% chance of eval vs training —
    # stable per id, independent of run seed
    evm = np.array([int(hashlib.md5(f'{f}'.encode()).hexdigest(), 16) % 10000
                    < args.holdout * 100 for f in lfid])
    ev = np.where(evm)[0]
    tr = np.where(~evm)[0]
    print(f'{label}: {len(tr)} train / {len(ev)} eval ({args.holdout:g}% id-seeded)')
    return tr, ev


def box_protect(cx, cy, half):
    # per-sample square keep-zone; everything else scrambles
    S = args.size
    yy, xx = torch.meshgrid(torch.arange(S, dtype=torch.float32, device=dev),
                            torch.arange(S, dtype=torch.float32, device=dev),
                            indexing='ij')
    m = ((xx[None] - cx[:, None, None]).abs() <= half[:, None, None]) \
      & ((yy[None] - cy[:, None, None]).abs() <= half[:, None, None])
    return m.float()[:, None]


def train_find(tw, laux, lfid, lpup, rec):
    # find only LOCATES: eye centers + scale. distance and xyz
    # come from target/look once locked onto the face
    yt = np.concatenate([laux[:, 0:4], laux[:, 8:9]], 1).astype(np.float32)
    tr, ev = split(lfid, lpup, 'find', clean_ok=False)
    x = torch.tensor(tw)
    y = torch.tensor(yt)
    xev = x[ev].to(dev)
    yev = y[ev].to(dev)
    # scramble everything but the labeled face box: the scene must
    # never carry the label, only the face may
    tex = torch.tensor(build_texpool()).to(dev)
    S = args.size
    vstats = level_stats(rec['wf']) if rec is not None else None

    def step(m):
        i = torch.randint(0, len(tr), (args.batch,))
        xb = x[tr][i].to(dev)
        yb = y[tr][i].to(dev)
        xb = renorm_levels(xb, vstats)
        if args.scramble > 0:
            cx = ((yb[:, 0] + yb[:, 2]) / 2 + 0.5) * S
            cy = ((yb[:, 1] + yb[:, 3]) / 2 + 0.5) * S
            xb = scramble(xb, box_protect(cx, cy, yb[:, 4] * 0.75 * S), tex)
        xb = sensor(xb)
        return F.mse_loss(m(xb), yb)

    loop(PointNet(-0.5, 0.5, 1), 'find',
         ['lx', 'ly', 'rx', 'ry', 'scale'],
         lambda e: e[:4].mean(), (len(tr), step),
         lambda m: (m(xev), yev),
         find_val(rec) if rec is not None else None)


def sub_window(mta, train):
    # window inside the context crop keeping both eyes visible;
    # train windows wander like a stale find, eval is centered 2x
    lo_u = torch.minimum(mta[:, 0], mta[:, 2]) - 0.06
    hi_u = torch.maximum(mta[:, 0], mta[:, 2]) + 0.06
    lo_v = torch.minimum(mta[:, 1], mta[:, 3]) - 0.06
    hi_v = torch.maximum(mta[:, 1], mta[:, 3]) + 0.06
    span = torch.maximum(hi_u - lo_u, hi_v - lo_v)
    fmin = torch.clamp(torch.maximum(torch.tensor(0.55, device=mta.device),
                                     span + 0.02), max=0.98)
    if not train:
        f = torch.maximum(fmin, torch.tensor(2.0 / args.ctx_mul, device=mta.device))
        u0 = torch.clamp((lo_u + hi_u) / 2 - f / 2, torch.zeros_like(f), 1 - f)
        v0 = torch.clamp((lo_v + hi_v) / 2 - f / 2, torch.zeros_like(f), 1 - f)
        return u0, v0, f
    r = lambda: torch.rand_like(fmin)
    f = fmin + r() * (0.9 - fmin).clamp(min=0)
    ua = (hi_u - f).clamp(min=0)
    ub = torch.maximum(ua, torch.minimum(lo_u, 1 - f))
    va = (hi_v - f).clamp(min=0)
    vb = torch.maximum(va, torch.minimum(lo_v, 1 - f))
    return ua + r() * (ub - ua), va + r() * (vb - va), f


def target_batch(cc, mta, train):
    # cut the window, remap labels into it
    B = cc.shape[0]
    u0, v0, f = sub_window(mta, train)
    theta = torch.zeros(B, 2, 3, device=cc.device)
    theta[:, 0, 0] = f
    theta[:, 1, 1] = f
    theta[:, 0, 2] = 2 * u0 + f - 1
    theta[:, 1, 2] = 2 * v0 + f - 1
    S = args.size
    grid = F.affine_grid(theta, (B, 1, S, S), align_corners=False)
    img = F.grid_sample(cc, grid, align_corners=False)
    off = torch.stack([u0, v0, u0, v0], 1)
    y = torch.cat([(mta[:, 0:4] - off) / f[:, None],
                   mta[:, 4:5] / f[:, None], mta[:, 5:9]], 1)
    return img, y


def train_target(tcc, tmt, lfid, lpup, rec):
    tr, ev = split(lfid, lpup, 'target')
    cc = torch.tensor(tcc)
    mt = torch.tensor(tmt)
    xev, yev = target_batch(cc[ev].to(dev), mt[ev].to(dev), False)
    tex = torch.tensor(build_texpool()).to(dev)
    S = args.size
    vstats = level_stats(rec['ctx']) if rec is not None else None

    def step(m):
        i = torch.randint(0, len(tr), (args.batch,))
        xb, yb = target_batch(cc[tr][i].to(dev), mt[tr][i].to(dev), True)
        xb = renorm_levels(xb, vstats)
        if args.scramble > 0:
            cx = (yb[:, 0] + yb[:, 2]) / 2 * S
            cy = (yb[:, 1] + yb[:, 3]) / 2 * S
            xb = scramble(xb, box_protect(cx, cy, yb[:, 4] * 0.75 * S), tex)
        xb = sensor(xb)
        return F.mse_loss(m(xb), yb)

    loop(PointNet(0.0, 1.0, 5), 'target',
         ['lx', 'ly', 'rx', 'ry', 'scale', 'hcx', 'hcy', 'hcz', 'dist'],
         lambda e: e[:4].mean(), (len(tr), step),
         lambda m: (m(xev), yev),
         target_val(rec) if rec is not None else None)


def gauss_kernel(sig, ksize=5):
    r = torch.arange(ksize, dtype=torch.float32) - ksize // 2
    g = torch.exp(-r * r / (2 * sig * sig))
    g = g / g.sum()
    return (g[:, None] * g[None]).reshape(1, 1, ksize, ksize).to(dev)


def eye_protect():
    # keep the eye center disc and the side corner bands clean —
    # they carry the annotated points; everything else scrambles
    S = args.size
    c = (S - 1) / 2
    yy, xx = torch.meshgrid(torch.arange(S, dtype=torch.float32),
                            torch.arange(S, dtype=torch.float32), indexing='ij')
    m = (xx - c) ** 2 + (yy - c) ** 2 <= (S * 0.22) ** 2
    m |= ((yy - c).abs() <= S * 0.19) & ((xx <= S * 0.22) | (xx >= S - 1 - S * 0.22))
    return m.float()[None, None].to(dev)


def face_protect(ab):
    # per-sample discs at eye centers + outer corners in the face
    # crop; the nose and everything else stays fair game
    S = args.size
    cx2, cy2, ss = ab[:, 15], ab[:, 16], ab[:, 17]
    yy, xx = torch.meshgrid(torch.arange(S, dtype=torch.float32, device=ab.device),
                            torch.arange(S, dtype=torch.float32, device=ab.device),
                            indexing='ij')
    m = torch.zeros(ab.shape[0], S, S, dtype=torch.bool, device=ab.device)
    for ix, iy in ((0, 1), (2, 3), (4, 5), (6, 7)):
        u = ((ab[:, ix] - cx2 + ss / 2) / ss * S)[:, None, None]
        v = ((ab[:, iy] - cy2 + ss / 2) / ss * S)[:, None, None]
        m |= (xx[None] - u) ** 2 + (yy[None] - v) ** 2 <= (S * 0.14) ** 2
    return m.float()[:, None]


def build_texpool():
    # donor patches from the real noise captures (noise: true) —
    # of 4 candidate windows, keep the one nearest the frame's
    # MEDIAN brightness: common noise, not highlights
    import glob
    S = args.size
    cache = f'/src/hyperspace-sessions/.texpool-v2-s{S}-n{args.tex_n}.npz'
    if os.path.exists(cache):
        return np.load(cache)['tex']
    from PIL import Image
    frames = []
    for agi in glob.glob('/src/hyperspace-sessions/*/record/*.agi'):
        text = open(agi).read()
        if 'noise: true' not in text:
            continue
        d = os.path.dirname(agi)
        fid = os.path.basename(agi)[:-4]
        for nm in ('top', 'bot'):
            p = os.path.join(d, f'{fid}-{nm}.png')
            if os.path.exists(p):
                frames.append(p)
    if not frames:
        raise SystemExit('no noise captures found for the texture pool')
    rng = np.random.RandomState(args.seed)
    per = args.tex_n // len(frames) + 1
    pats = []
    for p in frames:
        g = np.asarray(Image.open(p).convert('L'), np.float32) / 255.0
        fmed = float(np.median(g))
        H, W = g.shape
        for _ in range(per):
            best, bd = None, 1e9
            for _ in range(4):
                ts = rng.randint(12, min(65, H, W))
                x0 = rng.randint(0, W - ts)
                y0 = rng.randint(0, H - ts)
                c = g[y0:y0 + ts, x0:x0 + ts]
                d = abs(float(c.mean()) - fmed)
                if d < bd:
                    best, bd = c, d
            im = Image.fromarray((best * 255).astype(np.uint8))
            pats.append(np.asarray(im.resize((S, S), Image.BOX),
                                   np.float32)[None] / 255.0)
    tex = np.stack(pats[:args.tex_n])
    np.savez_compressed(cache, tex=tex)
    print(f'texture pool: {tex.shape[0]} patches from {len(frames)} noise frames')
    return tex


def perlin(B, S, device, cells=4):
    # two-octave value noise: bilinear-upsampled random grids.
    # cells sets the swath size — 2 = half-crop swaths, 6 = fine
    a = F.interpolate(torch.rand(B, 1, cells, cells, device=device), size=(S, S),
                      mode='bilinear', align_corners=False)
    b = F.interpolate(torch.rand(B, 1, cells * 2, cells * 2, device=device),
                      size=(S, S), mode='bilinear', align_corners=False)
    return 0.65 * a + 0.35 * b


def pinch_warp(x, protect):
    # perlin-field pinch/expand: every surface OUTSIDE the target
    # areas warps, displacement zeroed where labels must hold
    B, _, S, _ = x.shape
    amp = 0.08 + 0.22 * torch.rand(B, 1, 1, 1, device=x.device)
    dx = (perlin(B, S, x.device) - 0.5) * 2 * amp * (1 - protect)
    dy = (perlin(B, S, x.device) - 0.5) * 2 * amp * (1 - protect)
    lin = torch.linspace(-1, 1, S, device=x.device)
    gy, gx = torch.meshgrid(lin, lin, indexing='ij')
    grid = torch.stack([gx[None] + dx[:, 0], gy[None] + dy[:, 0]], -1)
    return F.grid_sample(x, grid, padding_mode='border', align_corners=False)


def scramble(x, protect, tex):
    # fuzz common-noise donor patches in through perlin alpha over
    # EVERYTHING — face and clips included. features are only
    # half-shielded. NO blur here: the sensor's field is the one
    # and only optics model
    B, _, S, _ = x.shape
    scr = pinch_warp(x, protect)
    for r in range(4):
        gate = (torch.rand(B, 1, 1, 1, device=x.device) < args.scramble).float()
        if r == 0:
            gate = torch.ones_like(gate)      # noise overlap in ALL
        d = tex[torch.randint(0, tex.shape[0], (B,), device=x.device)]
        # donor keeps its OWN structure contrast; only its mean
        # moves to the crop's — swaths must be visible
        dm = d.mean(dim=(1, 2, 3), keepdim=True)
        cm = scr.mean(dim=(1, 2, 3), keepdim=True)
        d = d - dm + cm
        # swath size varies per round: huge half-crop blobs to fine
        cells = int(torch.randint(2, 7, (1,)).item())
        a = perlin(B, S, x.device, cells)
        a = ((a - 0.25) * 2.2).clamp(0, 1) \
            * (0.15 + 0.4 * torch.rand(B, 1, 1, 1, device=x.device))
        scr = scr + gate * a * (d - scr)
    return x + (1 - 0.72 * protect) * (scr - x)


def sheet_cell(thumb, crops, caption):
    # one inspection cell: annotated frame, then the 3 net inputs
    from PIL import Image, ImageDraw
    cell = Image.new('RGB', (128 * 4, 140), (24, 24, 24))
    cell.paste(thumb.resize((128, 128), Image.BOX), (0, 0))
    for i, crop in enumerate(crops):
        ck = Image.fromarray((crop[0] * 255).astype(np.uint8)).convert('RGB')
        cell.paste(ck.resize((128, 128), Image.NEAREST), (128 * (i + 1), 0))
    dr = ImageDraw.Draw(cell)
    dr.text((2, 128), caption, fill=(0, 255, 0))
    return cell


def save_sheet(cells, path, label):
    from PIL import Image
    across = 2
    rows = (len(cells) + across - 1) // across
    sheet = Image.new('RGB', (128 * 4 * across, 140 * rows), (24, 24, 24))
    for i, cell in enumerate(cells):
        sheet.paste(cell, (128 * 4 * (i % across), 140 * (i // across)))
    sheet.save(path)
    print(f'{label} input sheet ({len(cells)} samples) -> {path}')


def training_sheet(session_dir, eye_l, eye_r, faces, laux, ly, lfid, lgeo,
                   rec, count=50):
    # same sheet as validation, from the synthetic training set —
    # crops shown AS TREATED (levels + scramble + blur), exactly
    # what the trainer eats
    from PIL import Image, ImageDraw
    idx = np.linspace(0, len(ly) - 1, min(count, len(ly))).astype(int)
    lb = torch.tensor(eye_l[idx]).to(dev)
    rb = torch.tensor(eye_r[idx]).to(dev)
    fb = torch.tensor(faces[idx]).to(dev)
    ab = torch.tensor(laux[idx]).to(dev)
    blended = None
    with torch.no_grad():
        if rec is not None:
            pick = torch.randint(0, len(rec['l']), (lb.shape[0],), device=dev)
            lb = renorm_levels(lb, level_stats(rec['l']), pick)
            rb = renorm_levels(rb, level_stats(rec['r']), pick)
            fb = renorm_levels(fb, level_stats(rec['f']), pick)
            # gated real-donor blend, shown on every matched cell
            dh = np.abs(ly[idx][:, None, 0:2] - rec['plots'][None, :, 0:2]).max(2)
            dg = np.abs(ly[idx][:, None, 2:4] - rec['plots'][None, :, 2:4]).max(2)
            rsc = rec['find_y'][:, 4]
            dsc = np.abs(laux[idx][:, 8:9] - rsc[None, :]) / rsc[None, :]
            ms = (laux[idx][:, 0:2] + laux[idx][:, 2:4]) / 2
            mr = (rec['find_y'][:, 0:2] + rec['find_y'][:, 2:4]) / 2
            dmid = np.abs(ms[:, None] - mr[None, :]).max(2)
            okb = torch.tensor((dh <= 0.15) & (dg <= 0.15) & (dsc <= 0.25)
                               & (dmid <= 0.25)).to(dev)
            has = okb.any(1)
            if bool(has.any()):
                dj = (torch.rand(okb.shape, device=dev) * okb).argmax(1)
                g9 = has.float()[:, None, None, None]
                cells9 = int(torch.randint(2, 5, (1,)).item())
                aL = ((perlin(len(idx), args.size, dev, cells9) - 0.25) * 2.2).clamp(0, 1) \
                    * (0.35 + 0.45 * torch.rand(len(idx), 1, 1, 1, device=dev))
                aR = ((perlin(len(idx), args.size, dev, cells9) - 0.25) * 2.2).clamp(0, 1) \
                    * (0.35 + 0.45 * torch.rand(len(idx), 1, 1, 1, device=dev))
                aF = ((perlin(len(idx), args.size, dev, cells9) - 0.25) * 2.2).clamp(0, 1) \
                    * (0.35 + 0.45 * torch.rand(len(idx), 1, 1, 1, device=dev))
                lb = lb + g9 * aL * (torch.tensor(rec['l']).to(dev)[dj] - lb)
                rb = rb + g9 * aR * (torch.tensor(rec['r']).to(dev)[dj] - rb)
                fb = fb + g9 * aF * (torch.tensor(rec['f']).to(dev)[dj] - fb)
                blended = has.cpu().numpy()
        if args.scramble > 0:
            tex = torch.tensor(build_texpool()).to(dev)
            lb = scramble(lb, eye_protect(), tex)
            rb = scramble(rb, eye_protect(), tex)
            fb = scramble(fb, face_protect(ab), tex)
        lb = sensor(lb, pupil_keep())
        rb = sensor(rb, pupil_keep())
        fb = sensor(fb)
    tl2 = lb.clamp(0, 1).cpu().numpy()
    tr2 = rb.clamp(0, 1).cpu().numpy()
    tf2 = fb.clamp(0, 1).cpu().numpy()
    cells = []
    for j, i in enumerate(idx):
        fid = int(lfid[i]) % 1000000
        p = os.path.join(session_dir, f'{fid}-face0.png')
        if not os.path.exists(p):
            continue
        img = Image.open(p).convert('RGB')
        W, H = img.size
        dr = ImageDraw.Draw(img)
        for bo, col in ((9, (255, 64, 64)), (12, (64, 128, 255)),
                        (15, (255, 255, 255))):
            cx, cy, s = laux[i, bo], laux[i, bo + 1], laux[i, bo + 2]
            x0 = (cx + 0.5) * W - s * W / 2
            y0 = (cy + 0.5) * H - s * W / 2
            dr.rectangle([x0, y0, x0 + s * W, y0 + s * W], outline=col, width=2)
        tag = ' REAL' if blended is not None and blended[j] else ''
        cap = (f'{fid} look {ly[i, 2]:+.2f} {ly[i, 3]:+.2f} '
               f'head {ly[i, 0]:+.2f} {ly[i, 1]:+.2f} d {lgeo[i, 3]:.3f}{tag}')
        cells.append(sheet_cell(img, (tl2[j], tr2[j], tf2[j]), cap))
    save_sheet(cells, os.path.join(session_dir, 'training-inputs.png'),
               'training')


def load_record(session_dir):
    # the real recordings: every annotated pair validates the look
    # model against the plots the person actually followed
    from PIL import Image, ImageDraw
    rd = os.path.join(session_dir, 'record')
    if not os.path.isdir(rd):
        return None
    print(f'loading validation images from {rd} ...')
    S = args.size
    out = {'l': [], 'r': [], 'f': [], 'aux': [], 'plots': [], 'dist': [],
           'wf': [], 'find_y': [], 'ctx': [], 'target_y': []}
    for fn in sorted(os.listdir(rd)):
        if not fn.endswith('.agi'):
            continue
        text = open(os.path.join(rd, fn)).read()
        if 'noise: true' in text:
            continue
        fid = fn[:-4]
        cams = int(read_pair(text, 'cameras')[0])
        for cs in ('top', 'bot')[:max(1, cams)]:
            ocl = read_pair(text, f'{cs}_left_oc')
            ocr = read_pair(text, f'{cs}_right_oc')
            nb  = read_pair(text, f'{cs}_nose_base')
            pl  = read_pair(text, f'{cs}_left_pupil')
            pr  = read_pair(text, f'{cs}_right_pupil')
            if min(ocl[0], ocr[0], nb[0], pl[0], pr[0]) < -900:
                continue
            ip = os.path.join(rd, f'{fid}-{cs}.png')
            if not os.path.exists(ip):
                continue
            img = Image.open(ip).convert('L')
            gray = np.asarray(img, np.float32) / 255.0
            H, W = gray.shape
            # record annotations are 0..1 frame fractions
            zc = lambda p: (p[0] - 0.5, p[1] - 0.5)
            ocl, ocr, nb, pl, pr = zc(ocl), zc(ocr), zc(nb), zc(pl), zc(pr)
            sc = float(np.hypot(ocl[0] - ocr[0], ocl[1] - ocr[1]))
            if sc <= 0.02:
                continue
            look = read_pair(text, 'look')
            head = read_pair(text, 'head')
            # the station era is over: recordings carry no true_dist
            td = read_pair(text, 'true_dist')[0]
            if td < -900:
                td = 0.0
            # pupils stand in for the un-annotated eye centers
            px_ = lambda p: ((p[0] + 0.5) * W, (p[1] + 0.5) * H)
            lxp, lyp = px_(pl)
            rxp, ryp = px_(pr)
            fside = sc * W * args.face_mul * FACE_PAD
            eside = sc * W * args.face_mul / args.eye_div * EYE_PAD
            lc, lx0, ly0, ls = crop_gray(gray, lxp, lyp, eside, S)
            rc, rx0, ry0, rs = crop_gray(gray, rxp, ryp, eside, S)
            fxp = (lxp + rxp + (nb[0] + 0.5) * W) / 3
            fyp = (lyp + ryp + (nb[1] + 0.5) * H) / 3
            fc, fx0, fy0, fs = crop_gray(gray, fxp, fyp, fside, S)
            def box(x0, y0, s):
                return [(x0 + s / 2) / W - 0.5, (y0 + s / 2) / H - 0.5, s / W]
            aux = ([pl[0], pl[1], pr[0], pr[1],
                    ocl[0], ocl[1], ocr[0], ocr[1], sc]
                   + box(lx0, ly0, ls) + box(rx0, ry0, rs) + box(fx0, fy0, fs))
            out['l'].append(lc)
            out['r'].append(rc)
            out['f'].append(fc)
            out['aux'].append(aux)
            out['plots'].append([head[0], head[1], look[0], look[1]])
            out['dist'].append(td)
            # find scores on the reduced frame: predicted eye centers
            # vs the annotated pupils, scale vs the oc span
            out['wf'].append(np.asarray(img.resize((S, S), Image.BOX),
                                        np.float32)[None] / 255.0)
            out['find_y'].append([pl[0], pl[1], pr[0], pr[1], sc])
            # target scores on a 2x-scale window at the oc midpoint
            mx, my = (ocl[0] + ocr[0]) / 2, (ocl[1] + ocr[1]) / 2
            tc, tx0, ty0, ts2 = crop_gray(gray, (mx + 0.5) * W, (my + 0.5) * H,
                                          sc * 2 * W, S)
            def win_frac(p):
                return (((p[0] + 0.5) * W - tx0) / ts2,
                        ((p[1] + 0.5) * H - ty0) / ts2)
            wlu, wlv = win_frac(pl)
            wru, wrv = win_frac(pr)
            out['ctx'].append(tc)
            out['target_y'].append([wlu, wlv, wru, wrv, sc * W / ts2])
            thumb = img.convert('RGB')
            tdr = ImageDraw.Draw(thumb)
            tdr.rectangle([lx0, ly0, lx0 + ls, ly0 + ls], outline=(255, 64, 64), width=2)
            tdr.rectangle([rx0, ry0, rx0 + rs, ry0 + rs], outline=(64, 128, 255), width=2)
            tdr.rectangle([fx0, fy0, fx0 + fs, fy0 + fs], outline=(255, 255, 255), width=2)
            out.setdefault('cells', []).append(sheet_cell(
                thumb, (lc, rc, fc),
                f'{fid}-{cs} look {look[0]:+.2f} {look[1]:+.2f} '
                f'head {head[0]:+.2f} {head[1]:+.2f} d {td:g}'))
    if not out['l']:
        return None
    save_sheet(out.pop('cells'), os.path.join(session_dir, 'validation-inputs.png'),
               'validation')
    return {k: np.array(v, np.float32) for k, v in out.items()}


def find_val(rec):
    # real-frame score: eye centers vs annotated pupils (nearest
    # annotated point per eye), scale vs the oc span
    x = torch.tensor(rec['wf']).to(dev)
    y = torch.tensor(rec['find_y']).to(dev)

    def fn(m):
        e = (m(x) - y).abs().mean(0)
        return ['v.lx', 'v.ly', 'v.rx', 'v.ry', 'v.scale'], [float(v) for v in e]
    return fn


def target_val(rec):
    # distance is not validated here: it transfers in from the
    # synthetic labels the real pixels blend into
    x = torch.tensor(rec['ctx']).to(dev)
    y = torch.tensor(rec['target_y']).to(dev)

    def fn(m):
        p = m(x)
        e = (p[:, :5] - y).abs().mean(0)
        return ['v.lx', 'v.ly', 'v.rx', 'v.ry', 'v.scale'], [float(v) for v in e]
    return fn


def look_val(rec):
    # distance is not validated here: it transfers in from the
    # synthetic labels the real pixels blend into
    t = lambda k: torch.tensor(rec[k]).to(dev)
    l, r, f2, a2 = t('l'), t('r'), t('f'), t('aux')
    plots = t('plots')
    z3 = torch.zeros(len(rec['dist']), 3, device=dev)

    def fn(m):
        p, _ = m(l, r, f2, a2, z3, z3)
        e = (p[:, :4] - plots).abs().mean(0)
        return (['v.head.x', 'v.head.y', 'v.gaze.x', 'v.gaze.y'],
                [float(v) for v in e])
    return fn


def train_look(tl, tr_, tf_, laux, ly, lfid, lpup, lgeo, rec):
    tr, ev = split(lfid, lpup, 'look')
    plt_ = lpup[:, [0, 1, 4]].astype(np.float32)
    prt_ = lpup[:, [2, 3, 4]].astype(np.float32)
    y8 = np.concatenate([ly, lgeo], 1).astype(np.float32)
    t = lambda a: torch.tensor(a)
    l, r, f2 = t(tl), t(tr_), t(tf_)
    a2, y2, pl2, pr2 = t(laux), t(y8), t(plt_), t(prt_)
    evd = [x[ev].to(dev) for x in (l, r, f2, a2, pl2, pr2)]
    yev = y2[ev].to(dev)
    tex = torch.tensor(build_texpool()).to(dev)
    print(f'donor textures: {tex.shape[0]}')
    eyem = eye_protect()
    vl = level_stats(rec['l']) if rec is not None else None
    vr = level_stats(rec['r']) if rec is not None else None
    vf = level_stats(rec['f']) if rec is not None else None
    # real PIXELS blend into samples whose face uv and eye uv both
    # agree within 0.15 of screen — labels never move; the crops
    # center on irises on both sides, so donors interpolate iris
    # to iris
    ok_t = None
    if rec is not None and args.real_blend > 0:
        dh = np.abs(ly[:, None, 0:2] - rec['plots'][None, :, 0:2]).max(2)
        dg = np.abs(ly[:, None, 2:4] - rec['plots'][None, :, 2:4]).max(2)
        rsc = rec['find_y'][:, 4]
        dsc = np.abs(laux[:, 8:9] - rsc[None, :]) / rsc[None, :]
        # eyes median must also sit in the same part of the frame
        ms = (laux[:, 0:2] + laux[:, 2:4]) / 2
        mr = (rec['find_y'][:, 0:2] + rec['find_y'][:, 2:4]) / 2
        dmid = np.abs(ms[:, None] - mr[None, :]).max(2)
        ok = (dh <= 0.15) & (dg <= 0.15) & (dsc <= 0.25) & (dmid <= 0.25)
        ok_t = torch.tensor(ok).to(dev)
        rld = torch.tensor(rec['l']).to(dev)
        rrd = torch.tensor(rec['r']).to(dev)
        rfd = torch.tensor(rec['f']).to(dev)
        ncov = int(ok.any(1).sum())
        print(f'real blend: {ncov}/{len(ly)} training samples have gated '
              f'donors ({100.0 * ncov / len(ly):.0f}% coverage)')
    tr_t = torch.tensor(tr)

    def step(m):
        i = torch.randint(0, len(tr), (args.batch,))
        lb, rb, fb, ab, plb, prb = [x[tr][i].to(dev)
                                    for x in (l, r, f2, a2, pl2, pr2)]
        # one validation sample sets levels, one blur for all 3
        pick = None
        if vl is not None:
            pick = torch.randint(0, vl.shape[0], (lb.shape[0],), device=dev)
        lb = renorm_levels(lb, vl, pick)
        rb = renorm_levels(rb, vr, pick)
        fb = renorm_levels(fb, vf, pick)
        if ok_t is not None:
            okb = ok_t[tr_t[i]]
            has = okb.any(1)
            if bool(has.any()):
                dj = (torch.rand(okb.shape, device=dev) * okb).argmax(1)
                g9 = (has & (torch.rand(len(i), device=dev) < args.real_blend)) \
                    .float()[:, None, None, None]
                cells9 = int(torch.randint(2, 5, (1,)).item())
                for _ in (0,):
                    aL = ((perlin(len(i), args.size, dev, cells9) - 0.25) * 2.2).clamp(0, 1) \
                        * (0.35 + 0.45 * torch.rand(len(i), 1, 1, 1, device=dev))
                    aR = ((perlin(len(i), args.size, dev, cells9) - 0.25) * 2.2).clamp(0, 1) \
                        * (0.35 + 0.45 * torch.rand(len(i), 1, 1, 1, device=dev))
                    aF = ((perlin(len(i), args.size, dev, cells9) - 0.25) * 2.2).clamp(0, 1) \
                        * (0.35 + 0.45 * torch.rand(len(i), 1, 1, 1, device=dev))
                lb = lb + g9 * aL * (rld[dj] - lb)
                rb = rb + g9 * aR * (rrd[dj] - rb)
                fb = fb + g9 * aF * (rfd[dj] - fb)
        if args.scramble > 0:
            lb = scramble(lb, eyem, tex)
            rb = scramble(rb, eyem, tex)
            fb = scramble(fb, face_protect(ab), tex)
        lb = sensor(lb, pupil_keep())
        rb = sensor(rb, pupil_keep())
        fb = sensor(fb)
        if args.aux_noise > 0:
            ab = ab + torch.randn_like(ab) * args.aux_noise
        p, ploss = m(lb, rb, fb, ab, plb, prb)
        return F.mse_loss(p, y2[tr][i].to(dev)) + ploss

    loop(LookNet(), 'look',
         ['head.x', 'head.y', 'gaze.x', 'gaze.y',
          'hcx', 'hcy', 'hcz', 'dist'],
         lambda e: e[2:4].mean(), (len(tr), step),
         lambda m: (m(*evd)[0], yev),
         look_val(rec) if rec is not None else None)


def main():
    np.random.seed(args.seed)
    torch.manual_seed(args.seed)
    names = [s.strip() for s in args.session.split(',') if s.strip()]
    if args.preview:
        for s in names:
            d = f'/src/hyperspace-sessions/{s}'
            for f in os.listdir(d):
                if f.startswith('.cache-'):
                    os.remove(os.path.join(d, f))
            load_session(d)
        print('previews rebuilt beside samples ({i}-preview.png)')
        return
    parts = [load_session(f'/src/hyperspace-sessions/{s}') for s in names]
    tl, tr_, tf_, tw = [np.concatenate([p[k] for p in parts]) for k in range(4)]
    laux = np.concatenate([p[4] for p in parts])
    ly   = np.concatenate([p[5] for p in parts])
    lfid = np.concatenate([p[6] + 1000000 * i for i, p in enumerate(parts)])
    lpup = np.concatenate([p[7] for p in parts])
    lgeo = np.concatenate([p[8] for p in parts])
    tcc  = np.concatenate([p[9] for p in parts])
    tmt  = np.concatenate([p[10] for p in parts])
    nvalid = int((lpup[:, 4] > 0).sum())
    print(f'pupil labels: {nvalid}/{len(lpup)} valid')
    # both inspection sheets, every training start
    sdir0 = f'/src/hyperspace-sessions/{names[0]}'
    rec = load_record(sdir0)
    if rec is not None:
        print(f'validation: {len(rec["dist"])} annotated recordings')
    training_sheet(sdir0, tl, tr_, tf_, laux, ly, lfid, lgeo, rec)
    if args.stats:
        print(f'== dataset: {args.session} ({len(ly)} samples) ==')
        for name, pts in (('gaze', ly[:, 2:4]), ('head', ly[:, :2])):
            print(f'  {name}: x {pts[:,0].min():+.3f}..{pts[:,0].max():+.3f}'
                  f'  y {pts[:,1].min():+.3f}..{pts[:,1].max():+.3f}')
        print(f'  scale: mean {laux[:,8].mean():.3f} span {laux[:,8].min():.3f}..{laux[:,8].max():.3f}')
        print(f'  dist:  mean {lgeo[:,3].mean():.3f} span {lgeo[:,3].min():.3f}..{lgeo[:,3].max():.3f}')
        return
    if args.process in ('all', 'find'):
        train_find(tw, laux, lfid, lpup, rec)
    if args.process in ('all', 'target'):
        train_target(tcc, tmt, lfid, lpup, rec)
    if args.process in ('all', 'look'):
        train_look(tl, tr_, tf_, laux, ly, lfid, lpup, lgeo, rec)


if __name__ == '__main__':
    main()
