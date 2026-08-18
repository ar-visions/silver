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
    p.add_argument('--session',  default='top2')
    p.add_argument('--epochs',   type=int,   default=100)
    p.add_argument('--lr',       type=float, default=0.00002)
    p.add_argument('--find_lr',  type=float, default=0.001)
    p.add_argument('--find_px',  type=int,   default=160)   # find frame px
    p.add_argument('--find_noise', type=int,   default=1)   # donors+blur+tone
    p.add_argument('--batch',    type=int,   default=64)
    p.add_argument('--draw',     type=int,   default=200000) # samples per epoch
    p.add_argument('--size',     type=int,   default=32)     # every net input side
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
    p.add_argument('--px_noise', type=float, default=0.004)   # halved
    # landmark scramble on the look crops: noise-capture patches
    # fuzzed in with perlin alpha, blur, smear — everywhere except
    # annotated points (eye centers, eye sides). prob per overlay
    p.add_argument('--scramble', type=float, default=0.14)  # his own pixels, was 0.7
    p.add_argument('--tex_n',    type=int,   default=4000)   # donor patch pool
    # optics blur radius on the frame at load, fraction of side
    p.add_argument('--blur',     type=float, default=0.0011)  # halved
    # pupil heat-map loss: DEAD since crops centered on irises —
    # the target became a constant center. 0 keeps train loss pure
    # mse, comparable with eval
    p.add_argument('--pupil_w',  type=float, default=0.0)
    p.add_argument('--holdout',  type=float, default=10.0)   # eval percent, id-seeded
    # tone variance that survives the nets' per-sample normalisation
    p.add_argument('--gamma',    type=float, default=0.9)    # exp(+-g): 0.41x .. 2.46x
    p.add_argument('--local_c',  type=float, default=0.5)    # local gain field amplitude
    p.add_argument('--tgt_rep',  type=int,   default=4)      # windows drawn per target crop
    p.add_argument('--tgt_rot',  type=float, default=25.0)   # target window roll, +/- deg
    p.add_argument('--find_rot', type=float, default=20.0)   # find canvas roll, +/- deg
    # perlin refraction: the resample reads through a bent field
    p.add_argument('--refract', type=float, default=0.035)   # displacement, normalized
    p.add_argument('--refract_cells', type=int, default=4)   # mid frequency
    # synthetic frames pass through the same sensor as reality:
    # downsampled to camera width before any crop is cut
    p.add_argument('--sensor_w', type=int,   default=340)
    # real validation crops blend into matched training samples
    # (look and head plots agree within 0.15): apply probability
    p.add_argument('--real_blend', type=float, default=0.5)
    # look: share of the sensor-allowed head slide along rig x, 0 = off
    p.add_argument('--slide',    type=float, default=1.0)
    p.add_argument('--stats',    action='store_true')
    p.add_argument('--preview',  action='store_true')
    return p.parse_args()

args = parse_args()
dev = torch.device('cuda' if torch.cuda.is_available() else 'cpu')

# standard crop pads — training, eval, validation all use these
EYE_PAD  = 1.25
FACE_PAD = 1.15
# find canvas: sensor frame centered in a PAD-times black canvas
PAD = 1.35


NUM = r'-?[\d.]+(?:[eE][+-]?\d+)?'

def read_pair(text, key):
    m = re.search(rf'{re.escape(key)}:\s*({NUM})(?:[ \t]+({NUM}))?', text)
    if not m:
        return (-999.0, -999.0)
    return (float(m.group(1)), float(m.group(2)) if m.group(2) else 0.0)


def read3(text, key):
    m = re.search(rf'{re.escape(key)}:\s*(-?[\d.eE+-]+)\s+(-?[\d.eE+-]+)\s+(-?[\d.eE+-]+)', text)
    return [float(m.group(i)) for i in (1, 2, 3)] if m else [0.0, 0.0, 0.0]


def read5(text, key):
    m = re.search(rf'{re.escape(key)}:\s*' + r'\s+'.join([f'({NUM})'] * 5), text)
    return [float(m.group(i)) for i in range(1, 6)] if m else [0.0] * 5


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


def obscure_box(mx, my, sc):
    # the share of the face box the sensor does NOT hold. it grades
    # over a whole face width and starts the moment the box touches
    # an edge, so the ramp is pixels wide instead of sub-pixel
    if sc <= 0:
        return 1.0
    def seen(m):
        return max(0.0, min(min(m + sc / 2, 0.5) - max(m - sc / 2, -0.5), sc))
    u = seen(mx) * seen(my) / (sc * sc)
    return float(min(1.0, max(0.0, 1.0 - u)))


def obscure_of(aux):
    # face box: outer-corner mid, side = the outer-corner span
    return obscure_box((aux[4] + aux[6]) * 0.5,
                       (aux[5] + aux[7]) * 0.5, aux[8])


def hint_plot(mx, my, sc):
    # the face-scale box slid to its closest overlap with the sensor
    lim = max(0.0, 0.5 - sc * 0.5)
    return [min(max(mx, -lim), lim) / PAD, min(max(my, -lim), lim) / PAD]


def load_one(job):
    session_dir, fid = job
    text = open(os.path.join(session_dir, f'{fid}.agi')).read()
    cams = int(read_pair(text, 'cameras')[0])
    ncam = max(cams, 0)
    per = [load_cam(session_dir, fid, text, str(c)) for c in range(ncam)]
    # find/target stay per-view: a detector only ever sees one image.
    # hidden views load as None and never become rows
    out = [s for s in per if s]
    # look is geometric and needs the views TOGETHER — one row per pose
    # carrying both, each with its own obscure factor
    pair = None
    if out:
        S = args.size
        zc = np.zeros((1, S, S), np.float32)
        za = np.zeros(NAUX, np.float32)
        zp = np.zeros(3, np.float32)
        def part(i):
            if i < len(per) and per[i]:
                lc, rc, fc, wf, aux, y, f2, pup, geo, cc, tm, sf = per[i]
                return (lc, rc, fc, np.asarray(aux, np.float32), y, geo,
                        np.asarray([pup[0], pup[1], pup[4]], np.float32),
                        np.asarray([pup[2], pup[3], pup[4]], np.float32),
                        obscure_of(aux))
            # no eye labels for this view — feed the real frame anyway.
            # a turned head still fixes yaw, and an empty frame still
            # carries lighting and occlusion. obscure says which it is
            wf = load_view_pixels(session_dir, fid, str(i))
            if wf is None:
                return (zc, zc, zc, za, None, None, zp, zp, 1.0)
            return (wf, wf, wf, za, None, None, zp, zp, 1.0)
        p0, p1 = part(0), part(1)
        yy = p0[4] if p0[4] is not None else p1[4]
        g0 = p0[5] if p0[5] is not None else p1[5]
        g1 = p1[5] if p1[5] is not None else p0[5]
        # geometry: head_center xyz is shared, distance is per camera
        geo2 = [g0[0], g0[1], g0[2], g0[3], g1[3]]
        aux2 = np.concatenate([p0[3], p1[3], [p0[8], p1[8]]]).astype(np.float32)
        # rig: screen size then x y z tilt fov per camera. this is what
        # turns a frame slide into meters of head travel
        scr = read_pair(text, 'screen')
        rig = [scr[0], scr[1]] + read5(text, 'cam0') + read5(text, 'cam1')
        pair = (p0[0], p0[1], p0[2], p1[0], p1[1], p1[2], aux2, yy, fid,
                np.asarray(geo2, np.float32), p0[6], p0[7], p1[6], p1[7],
                np.asarray(rig, np.float32))
    return out, pair


def load_view_pixels(session_dir, fid, sfx):
    """Crops for a view with no eye labels — the back of a head, or an
    empty frame. There is nothing to centre on, so all three inputs are
    the reduced whole frame: still real pixels carrying head yaw,
    lighting and occlusion for the other view to lean on."""
    from PIL import Image
    ip = os.path.join(session_dir, f'{fid}-face{sfx}.png')
    if not os.path.exists(ip):
        return None
    img = Image.open(ip).convert('L')
    if args.sensor_w > 0 and img.width > args.sensor_w:
        img = img.resize((args.sensor_w, args.sensor_w), Image.BOX)
    wf = np.asarray(img.resize((args.size, args.size), Image.BOX),
                    np.float32)[None] / 255.0
    return wf


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
    # find input: the frame centered in the PAD-times black canvas
    wf, _, _, _ = crop_gray(gray, W / 2, H / 2, W * PAD, args.size)
    # and the bare frame, big enough that find can PLACE the face and
    # still downsample once, the way a real capture arrives
    sf = np.asarray(Image.fromarray((gray * 255).astype(np.uint8))
                    .resize((args.find_px, args.find_px), Image.BOX),
                    np.float32)[None] / 255.0
    # context crop: eye-mid centered, then CLAMPED onto the sensor —
    # an off-frame face still yields a real-pixel hint crop, and the
    # net confirms the obscure amount from those hints
    ccx, ccy = (lx + rx) / 2, (lyv + ryv) / 2
    ccs = sc * W * args.ctx_mul
    ccx = W / 2 if ccs >= W else min(max(ccx, ccs / 2), W - ccs / 2)
    ccy = H / 2 if ccs >= H else min(max(ccy, ccs / 2), H - ccs / 2)
    cc, cx0, cy0, cs = crop_gray(gray, ccx, ccy, ccs, args.ctx_size)
    # target labels: eyes as 0..1 context fractions, scale as a
    # context-side fraction, obscure. geometry belongs to look alone
    def ctx_frac(p):
        return (((p[0] + 0.5) * W - cx0) / cs, ((p[1] + 0.5) * H - cy0) / cs)
    clu, clv = ctx_frac(pl)
    cru, crv = ctx_frac(pr)
    omx = (ocl[0] + ocr[0]) * 0.5
    omy = (ocl[1] + ocr[1]) * 0.5
    obt = obscure_box(omx, omy, sc)
    tmeta = [clu, clv, cru, crv, sc * W / cs, obt]
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
    # {i}-preview.png: labels drawn on the PADDED find canvas, so
    # off-frame points land in the black pad instead of vanishing —
    # this is how obscured samples are inspected
    side = int(W * PAD)
    ox, oy = (side - W) // 2, (side - H) // 2
    pim = Image.new('RGB', (side, side), (0, 0, 0))
    pim.paste(img.convert('RGB'), (ox, oy))
    dr = ImageDraw.Draw(pim)
    def cross(x, y2, col, r2=4):
        px, py = (x + 0.5) * W + ox, (y2 + 0.5) * H + oy
        dr.line([px - r2, py, px + r2, py], fill=col, width=2)
        dr.line([px, py - r2, px, py + r2], fill=col, width=2)
    cross(pl[0], pl[1], (0, 255, 0))
    cross(pr[0], pr[1], (0, 255, 0))
    cross(ocl[0], ocl[1], (0, 255, 255))
    cross(ocr[0], ocr[1], (0, 255, 255))
    if nb[0] > -900:
        cross(nb[0], nb[1], (255, 255, 0))
    dr.rectangle([ox, oy, ox + W, oy + H], outline=(90, 90, 90), width=1)
    dr.rectangle([lx0 + ox, ly0 + oy, lx0 + ls + ox, ly0 + ls + oy],
                 outline=(255, 64, 64), width=2)
    dr.rectangle([rx0 + ox, ry0 + oy, rx0 + rs + ox, ry0 + rs + oy],
                 outline=(64, 128, 255), width=2)
    dr.rectangle([fx0 + ox, fy0 + oy, fx0 + fs + ox, fy0 + fs + oy],
                 outline=(255, 255, 255), width=2)
    pim = pim.resize((512, 512), Image.BOX)
    dr = ImageDraw.Draw(pim)
    dr.text((6, 6), f'head {head[0]:+.3f} {head[1]:+.3f}  '
                    f'gaze {look[0]:+.3f} {look[1]:+.3f}  scale {sc:.3f}  '
                    f'dist {cam_dist:.3f}  obscure {obt:.2f}',
            fill=(0, 255, 0))
    pim.save(os.path.join(session_dir, f'{fid}-preview{sfx}.png'))
    # the finished inputs (left | right | face), 8x nearest
    strip = np.concatenate([lc, rc, fc], axis=2)[0]
    simg = Image.fromarray((strip * 255).astype(np.uint8))
    simg = simg.resize((simg.width * 8, simg.height * 8), Image.NEAREST)
    simg.save(os.path.join(session_dir, f'{fid}-inputs{sfx}.png'))
    return lc, rc, fc, wf, aux, y, fid, pup, geo, cc, tmeta, sf


def load_session(session_dir):
    ids = sorted(int(f[:-4]) for f in os.listdir(session_dir)
                 if f.endswith('.agi') and f[:-4].isdigit())
    if not ids:
        raise SystemExit(f'no .agi samples in {session_dir}')
    newest = max(max(os.path.getmtime(os.path.join(session_dir, f'{i}.agi')) for i in ids),
                 os.path.getmtime(session_dir))
    cache = os.path.join(session_dir,
        f'.cache-v19-b{args.blur}-e{args.eye_div}-f{args.face_mul}'
        f'-p{args.find_px}'
        f'-w{args.sensor_w}'
        f'-s{args.size}-c{args.ctx_mul}-x{args.ctx_size}.npz')
    keys = ('tl', 'tr', 'tf', 'tw', 'laux', 'ly', 'lfid', 'lpup',
            'lgeo', 'tcc', 'tmt', 'tsf')
    # paired look set: both views of one pose in a single row
    pkeys = ('pl0', 'pr0', 'pf0', 'pl1', 'pr1', 'pf1', 'paux', 'py',
             'pfid', 'pgeo', 'ppl0', 'ppr0', 'ppl1', 'ppr1', 'prig')
    if os.path.exists(cache) and os.path.getmtime(cache) >= newest:
        z = np.load(cache)
        return [z[k] for k in keys], [z[k] for k in pkeys]
    print(f'loading {len(ids)} samples ...')
    from concurrent.futures import ProcessPoolExecutor
    with ProcessPoolExecutor() as ex:
        got = list(ex.map(load_one, [(session_dir, i) for i in ids], chunksize=16))
    rows  = [s for group, _ in got for s in group]
    pairs = [pr for _, pr in got if pr is not None]
    cols = list(zip(*rows))
    r = [np.array(c, np.float32) if k != 'lfid' else np.array(c)
         for k, c in zip(keys, cols)]
    pcols = list(zip(*pairs))
    pr_ = [np.array(c, np.float32) if k != 'pfid' else np.array(c)
           for k, c in zip(pkeys, pcols)]
    print(f'look pairs: {len(pairs)} poses '
          f'({int((pr_[pkeys.index("paux")][:, -2:] < 1).all(1).sum())} with both views)')
    np.savez_compressed(cache, **dict(zip(keys, r)), **dict(zip(pkeys, pr_)))
    return r, pr_


NAUX  = 18            # per view
NAUX2 = 2 * NAUX + 2  # both views + visible0/visible1


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
            nn.Linear(NAUX2, 128), nn.ReLU(),
            nn.Linear(128, 128), nn.ReLU(), nn.Linear(128, 2))
        hi_last = nn.Linear(128, 2)
        nn.init.zeros_(hi_last.weight)
        nn.init.zeros_(hi_last.bias)
        # face features of BOTH views feed the image correction
        self.head_img = nn.Sequential(nn.Linear(2 * fe, 128), nn.ReLU(), hi_last)
        # 4 eye embeddings + 2 face embeddings + aux + head
        self.delta = nn.Sequential(
            nn.Linear(6 * fe + NAUX2 + 2, 128), nn.ReLU(), nn.Linear(128, 2))
        # geometry head: head_center xyz + camera distance, aux-led
        # with a zero-init image correction like head
        # head_center xyz shared + distance to EACH camera: the pair is
        # what makes depth observable at all
        gi_last = nn.Linear(128, 5)
        nn.init.zeros_(gi_last.weight)
        nn.init.zeros_(gi_last.bias)
        self.geo_aux = nn.Sequential(
            nn.Linear(NAUX2, 128), nn.ReLU(),
            nn.Linear(128, 128), nn.ReLU(), nn.Linear(128, 5))
        self.geo_img = nn.Sequential(nn.Linear(2 * fe, 128), nn.ReLU(), gi_last)

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

    def forward(self, l0, r0, f0, l1, r1, f1, a, pl0, pr0, pl1, pr1):
        # one encoder walks both views; a blind view arrives as zeros and
        # its visible flag in `a` tells the net to discount it
        fc0, fc1 = self.face_enc(f0), self.face_enc(f1)
        el0, hml0 = self.eye(l0, want_hm=True)
        er0, hmr0 = self.eye(r0, want_hm=True)
        el1, hml1 = self.eye(l1, want_hm=True)
        er1, hmr1 = self.eye(r1, want_hm=True)
        fcat = torch.cat([fc0, fc1], 1)
        head = self.head_aux(a) + self.head_img(fcat)
        hsg = head.detach()
        d = self.delta(torch.cat([el0, er0, el1, er1, fc0, fc1, a, hsg], 1))
        geo = self.geo_aux(a) + self.geo_img(fcat)
        ploss = 0.0
        if self.training and args.pupil_w > 0:
            # pupil supervision stays per view — that is why the views cannot
            # simply be stacked into channels
            ploss = args.pupil_w * (
                self.pupil_map_loss(hml0, pl0[:, 0], pl0[:, 1], pl0[:, 2])
              + self.pupil_map_loss(hmr0, pr0[:, 0], pr0[:, 1], pr0[:, 2])
              + self.pupil_map_loss(hml1, pl1[:, 0], pl1[:, 1], pl1[:, 2])
              + self.pupil_map_loss(hmr1, pr1[:, 0], pr1[:, 1], pr1[:, 2]))
        # [face uv, gaze uv, head_center xyz, dist cam0, dist cam1]
        return torch.cat([head, hsg + d, geo], 1), ploss


class PointNet(nn.Module):
    # shared find/target shape: k supervised soft-argmax channels
    # + pooled head. find: oc corners + nose, scale + obscured;
    # target: 2 eye centers + scale — geometry belongs to look
    def __init__(self, lo, hi, pooled_out, k=2, ch=(8, 16, 16), fc=32):
        super().__init__()
        S = args.size
        self.k = k
        a, b, c = ch
        # the heat map is a 1x1 conv, so a point channel can only tell
        # left eye from right if the trunk's receptive field spans both.
        # 5+3+3 reaches 9px and the eyes sit ~13px apart at size 32 —
        # the two channels then collapse onto the midpoint. dilation
        # takes the field to 35px, the whole window.
        self.trunk = nn.Sequential(
            nn.Conv2d(1, a, 5, padding=2), nn.ReLU(),
            nn.Conv2d(a, b, 3, padding=1), nn.ReLU(),
            nn.Conv2d(b, c, 3, padding=2, dilation=2), nn.ReLU(),
            nn.Conv2d(c, c, 3, padding=4, dilation=4), nn.ReLU(),
            nn.Conv2d(c, c, 3, padding=8, dilation=8), nn.ReLU())
        self.heat = nn.Conv2d(c, k, 1)
        self.temp = nn.Parameter(torch.tensor(8.0))
        self.register_buffer('lin', torch.linspace(lo, hi, S))
        self.app = None if pooled_out == 0 else nn.Sequential(
            nn.MaxPool2d(2), nn.Conv2d(c, c, 3, padding=1), nn.ReLU(),
            nn.MaxPool2d(2), nn.Conv2d(c, c, 3, padding=1), nn.ReLU(),
            nn.Flatten(), nn.Linear(c * (S // 4) ** 2, fc), nn.ReLU(),
            nn.Linear(fc, pooled_out))

    def forward(self, x):
        t = self.trunk(norm_input(x))
        hm = self.heat(t)
        _, xs, ys = soft_argmax(hm, self.temp, self.lin)
        parts = []
        for i in range(self.k):
            parts += [xs[:, i:i + 1], ys[:, i:i + 1]]
        if self.app is not None:
            parts.append(self.app(t))
        return torch.cat(parts, 1)


_blur_bank = None

def blur_bank():
    # blur levels 0.11..2.2% of the side — the FIELD interpolates
    # between these per pixel
    global _blur_bank
    if _blur_bank is None:
        _blur_bank = []
        for f in np.linspace(0.0011, 0.022, 6):
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


def photometric(x):
    # 32x32 carries little structure, so vary tone hard. gamma and a
    # local gain field both SURVIVE the per-sample mean/std norm the
    # nets apply; a plain brightness or gain shift does not.
    B, _, S, _ = x.shape
    g = torch.exp((torch.rand(B, 1, 1, 1, device=x.device) * 2 - 1) * args.gamma)
    x = x.clamp(0.002, 1.0) ** g
    if args.local_c > 0:
        f = perlin(B, S, x.device, cells=3)
        x = x * (1.0 + (f * 2 - 1) * args.local_c)
    return x.clamp(0.0, 1.0)


def sensor(x, keep=None):
    # a true blur FIELD: perlin picks the local blur strength per
    # PIXEL; `keep` regions retain most of their pre-blur detail
    bank = blur_bank()
    K = len(bank)
    x = photometric(x)
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


def loop(model, name, cols, metric, steps_data, eval_data, val_fn=None,
         lr=None):
    model.to(dev)
    opt = torch.optim.Adam(model.parameters(), lr=lr or args.lr)
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
    # torchscript face: both views in one call —
    # (l0, r0, f0, l1, r1, f1, aux) -> 9 outputs, no pupil args.
    # a blind camera passes zero crops and obscure 1 in aux
    def __init__(self, m):
        super().__init__()
        self.m = m

    def forward(self, l0, r0, f0, l1, r1, f1, a):
        z = torch.zeros(l0.shape[0], 3, device=l0.device)
        out, _ = self.m(l0, r0, f0, l1, r1, f1, a, z, z, z, z)
        return out


def export_ts(model, name, out):
    # the app runs these live through the torchshim (.ptc)
    S = args.size
    m2 = model.to('cpu').eval()
    one = torch.zeros(1, 1, S, S)
    with torch.no_grad():
        if isinstance(m2, LookNet):
            w = LookWrap(m2)
            ts = torch.jit.trace(w, (one, one, one, one, one, one,
                                     torch.zeros(1, NAUX2)))
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




def obscure_t(m_c, sc_c):
    # torch twin of obscure_box: share of the face box off the sensor.
    # inputs are canvas fractions, the sensor is 1/PAD of the canvas
    m = m_c * PAD
    sc = (sc_c[:, 0] * PAD).clamp(min=1e-6)
    def seen(k):
        return (torch.minimum(m[:, k] + sc / 2, torch.full_like(sc, 0.5))
                - torch.maximum(m[:, k] - sc / 2, torch.full_like(sc, -0.5))
                ).clamp(min=0).minimum(sc)
    return (1.0 - seen(0) * seen(1) / (sc * sc)).clamp(0, 1)


def find_place(mid, sc, edge, u, other):
    # put the face where a chosen share u of it is off ONE edge, the
    # other axis fully in view. edge -1 keeps the whole face in.
    # returns the shift, in sensor fractions, that takes it there
    lim = (0.5 - sc / 2).clamp(min=0.0)
    off = 0.5 + sc / 2 - sc * (1.0 - u)
    o = other * lim
    tx = torch.where(edge == 0, off, torch.where(edge == 1, -off, o))
    ty = torch.where(edge == 2, off, torch.where(edge == 3, -off, o))
    tx = torch.where(edge < 0, u * lim, tx)
    ty = torch.where(edge < 0, other * lim, ty)
    return torch.stack([tx - mid[:, 0], ty - mid[:, 1]], 1)


def train_find(tsf, laux, lfid, rec):
    # find: ONE plot per padded frame — the face box slid to its
    # closest overlap with the sensor — plus scale and obscure, the
    # share of the box the sensor does not hold. each draw PLACES the
    # face at a chosen obscure amount on a chosen edge, so the label
    # covers 0..1 evenly instead of piling up at the ends. only faces
    # wholly inside their source frame train: no shift can restore
    # pixels the camera never captured
    S = args.size
    ms = np.stack([(laux[:, 4] + laux[:, 6]) * 0.5,
                   (laux[:, 5] + laux[:, 7]) * 0.5], 1)
    ss = laux[:, 8]
    whole = (np.abs(ms).max(1) + ss / 2) <= 0.5
    tr, ev = split(lfid, None, 'find')
    tr = np.array([i for i in tr if whole[i]])
    ev = np.array([i for i in ev if whole[i]])
    print(f'find: {len(tr)} train / {len(ev)} eval views wholly in frame '
          f'of {len(laux)} labelled')
    x = torch.tensor(tsf)
    m0 = torch.tensor((ms / PAD).astype(np.float32))
    s0 = torch.tensor((ss[:, None] / PAD).astype(np.float32))
    tex = torch.tensor(build_texpool()).to(dev)
    vstats = level_stats(rec['wf']) if rec is not None else None
    lin1 = torch.linspace(-1.0, 1.0, S, device=dev)
    wmask = ((lin1.abs() <= 1.0 / PAD).float()[None, :]
             * (lin1.abs() <= 1.0 / PAD).float()[:, None])[None, None]
    C = int(round(args.find_px * PAD))

    def compose(sfb, mb, sb, sh, rot=None):
        # place the frame in the canvas at FRAME resolution, roll it,
        # fill whatever it vacates with a face-free donor patch, then
        # reduce once — one clean downsample, the way a capture arrives
        B = sfb.shape[0]
        if rot is None:
            rot = torch.zeros(B, device=dev)
        ca, sa = torch.cos(rot), torch.sin(rot)
        th = torch.zeros(B, 2, 3, device=dev)
        th[:, 0, 0] = PAD * ca
        th[:, 0, 1] = -PAD * sa
        th[:, 1, 0] = PAD * sa
        th[:, 1, 1] = PAD * ca
        th[:, 0, 2] = -2 * sh[:, 0] * PAD
        th[:, 1, 2] = -2 * sh[:, 1] * PAD
        g = F.affine_grid(th, (B, 1, C, C), align_corners=False)
        fld = None
        if args.refract > 0:
            fld = refract(B, C, dev, args.refract, args.refract_cells)
            g = g + fld.permute(0, 2, 3, 1)
        big = F.grid_sample(sfb, g, align_corners=False)
        cov = F.grid_sample(torch.ones_like(sfb), g, align_corners=False)
        d0 = tex[torch.randint(0, tex.shape[0], (B,), device=dev)]
        big = torch.where(cov > 0.5, big,
                          F.interpolate(d0, size=(C, C), mode='nearest'))
        xb = F.interpolate(big, size=(S, S), mode='area') * wmask
        # the plot turns with the content, about the canvas centre
        u = mb[:, 0] + sh[:, 0]
        v = mb[:, 1] + sh[:, 1]
        mb = torch.stack([u * ca + v * sa, v * ca - u * sa], 1)
        if fld is not None:
            o = (mb * 2)[:, None, :]
            mb = refract_pts(fld, o)[:, 0, :] / 2
        lim = (0.5 / PAD - sb * 0.5).clamp(min=0.0)
        pb = torch.maximum(torch.minimum(mb, lim), -lim)
        return xb, mb, torch.cat([pb, sb, obscure_t(mb, sb)[:, None]], 1)

    # eval is a fixed grid: every held-out view on all four edges at
    # five obscure amounts, so the score covers the whole range
    with torch.no_grad():
        xs, ys = [], []
        for e9 in (-1, 0, 1, 2, 3):
            for u9 in np.linspace(0.0, 1.0, 5):
                n = len(ev)
                mb = m0[ev].to(dev) * PAD
                sb = s0[ev].to(dev) * PAD
                sh = find_place(mb, sb[:, 0],
                                torch.full((n,), e9, device=dev),
                                torch.full((n,), float(u9), device=dev),
                                torch.full((n,), 0.4, device=dev)) / PAD
                xe, _, ye = compose(x[ev].to(dev), m0[ev].to(dev),
                                    s0[ev].to(dev), sh)
                xs.append(xe)
                ys.append(ye)
        xev, yev = torch.cat(xs), torch.cat(ys)
        ob = yev[:, 3].cpu().numpy()
        print(f'find eval draws {len(xev)}: obscure '
              f'{100*(ob<=.001).mean():.0f}% at 0, '
              f'{100*((ob>.001)&(ob<.999)).mean():.0f}% graded, '
              f'{100*(ob>=.999).mean():.0f}% at 1')

    def step(m):
        i = torch.randint(0, len(tr), (args.batch,))
        j = torch.tensor(tr)[i]
        xb = x[j].to(dev)
        mb = m0[j].to(dev)
        sb = s0[j].to(dev)
        B = xb.shape[0]
        # a third of the draws sit in view, the rest leave by an edge
        edge = torch.randint(0, 4, (B,), device=dev)
        edge = torch.where(torch.rand(B, device=dev) < 0.34,
                           torch.full_like(edge, -1), edge)
        u = torch.rand(B, device=dev)
        u = torch.where(edge < 0, torch.rand(B, device=dev) * 2 - 1, u)
        sh = find_place(mb * PAD, sb[:, 0] * PAD, edge, u,
                        torch.rand(B, device=dev) * 2 - 1) / PAD
        rot = (torch.rand(B, device=dev) * 2 - 1) * np.radians(args.find_rot)
        xb, mb, yb = compose(xb, mb, sb, sh, rot)
        # clone noise donors over random parts that are not the face
        keep = box_protect((mb[:, 0] + 0.5) * S, (mb[:, 1] + 0.5) * S,
                           sb[:, 0] * 0.75 * S)
        for _ in range(3 if args.find_noise else 0):
            d = tex[torch.randint(0, tex.shape[0], (B,), device=dev)]
            a = ((perlin(B, S, dev, int(torch.randint(2, 6, (1,)).item()))
                  - 0.35) * 2.5).clamp(0, 1)
            gate = (torch.rand(B, 1, 1, 1, device=dev) < 0.7).float()
            xb = xb + gate * a * (1 - keep) * (d - xb)
        if args.find_noise:
            xb = renorm_levels(xb, vstats)
            xb = sensor(xb)
        return F.mse_loss(m(xb), yb)

    loop(PointNet(-0.5, 0.5, 2, k=1, ch=(16, 32, 32), fc=64), 'find',
         ['x', 'y', 'scale', 'obscure'],
         lambda e: e[:2].mean(), (len(tr), step),
         lambda m: (m(xev), yev),
         find_val(rec) if rec is not None else None, lr=args.find_lr)


def sub_window(mta, train):
    # window inside the context crop keeping both eyes visible;
    # train windows wander like a stale find, eval is centered 2x
    # margin varies per sample: at the low end an eye sits right on the
    # window edge, which is what a stale find plot actually hands over
    if train:
        # never below 0: a pinned eye label carries no position, and the
        # net cannot learn it — that is noise, not augmentation
        mg = (torch.rand_like(mta[:, 0]) * 0.12 + 0.005)[:, None]
    else:
        mg = torch.full_like(mta[:, 0:1], 0.06)
    lo_u = torch.minimum(mta[:, 0], mta[:, 2])[:, None] - mg
    hi_u = torch.maximum(mta[:, 0], mta[:, 2])[:, None] + mg
    lo_v = torch.minimum(mta[:, 1], mta[:, 3])[:, None] - mg
    hi_v = torch.maximum(mta[:, 1], mta[:, 3])[:, None] + mg
    lo_u, hi_u, lo_v, hi_v = lo_u[:, 0], hi_u[:, 0], lo_v[:, 0], hi_v[:, 0]
    span = torch.maximum(hi_u - lo_u, hi_v - lo_v)
    fmin = torch.clamp(torch.maximum(torch.tensor(0.42, device=mta.device),
                                     span + 0.02), max=0.98)
    if not train:
        f = torch.maximum(fmin, torch.tensor(2.0 / args.ctx_mul, device=mta.device))
        u0 = torch.clamp((lo_u + hi_u) / 2 - f / 2, torch.zeros_like(f), 1 - f)
        v0 = torch.clamp((lo_v + hi_v) / 2 - f / 2, torch.zeros_like(f), 1 - f)
        return u0, v0, f
    r = lambda: torch.rand_like(fmin)
    f = fmin + r() * (1.0 - fmin).clamp(min=0)
    ua = (hi_u - f).clamp(min=0)
    ub = torch.maximum(ua, torch.minimum(lo_u, 1 - f))
    va = (hi_v - f).clamp(min=0)
    vb = torch.maximum(va, torch.minimum(lo_v, 1 - f))
    return ua + r() * (ub - ua), va + r() * (vb - va), f


def refract(B, S, device, amp, cells=3):
    # a smooth displacement field in normalized units — the resample
    # reads through it, so the image bends the way it would through
    # moving air or a soft lens rather than being cut and pasted
    dx = (perlin(B, S, device, cells) - 0.5) * 2 * amp
    dy = (perlin(B, S, device, cells) - 0.5) * 2 * amp
    return torch.cat([dx, dy], 1)


def refract_pts(fld, o, iters=2):
    # where a feature ENDS UP after the bend. the field is defined on
    # output coords, so invert it by fixed point — two passes is ample
    # for a field this smooth, and the label must follow the pixels.
    p = o
    for _ in range(iters):
        v = F.grid_sample(fld, p[:, :, None, :], align_corners=False)
        p = o - v[..., 0].permute(0, 2, 1)
    return p


def carry_pts(src, grid, pu, pv, S, fb_u, fb_v):
    # a target is just a pixel finding its location: plant a bump where
    # the label is and push it through the SAME resample as the image.
    # no inverse anywhere, so there is no sign to get wrong. labels that
    # land outside the window keep the analytic value and clamp.
    Ci = src.shape[-1]
    lin = torch.linspace(-1, 1, Ci, device=src.device)
    xx, yy = lin[None, None, None, :], lin[None, None, :, None]
    sig = 3.0 / Ci
    px = (pu * 2 - 1)[..., None, None]
    py = (pv * 2 - 1)[..., None, None]
    m = torch.exp(-(((xx - px) ** 2 + (yy - py) ** 2) / (2 * sig * sig)))
    w = F.grid_sample(m, grid, align_corners=False)
    ls = torch.linspace(0, 1, S, device=src.device)
    mass = w.sum((2, 3))
    cx = (w.sum(2) * ls).sum(2) / mass.clamp(min=1e-9)
    cy = (w.sum(3) * ls).sum(2) / mass.clamp(min=1e-9)
    inside = mass > 1e-3
    return torch.where(inside, cx, fb_u), torch.where(inside, cy, fb_v)


def target_batch(cc, mta, train):
    # cut the window, ROLL it, remap labels into it. the context crop is
    # interior, so a rolled window is exactly a rolled head — free
    # variance from data we already have.
    B = cc.shape[0]
    u0, v0, f = sub_window(mta, train)
    if train and args.tgt_rot > 0:
        a = (torch.rand(B, device=cc.device) * 2 - 1) * np.radians(args.tgt_rot)
    else:
        a = torch.zeros(B, device=cc.device)
    ca, sa = torch.cos(a), torch.sin(a)
    # shrink so the rolled window stays inside the context: no black
    # corners, which would be a synthetic-only tell
    fr = f / (ca.abs() + sa.abs())
    theta = torch.zeros(B, 2, 3, device=cc.device)
    theta[:, 0, 0] = fr * ca
    theta[:, 0, 1] = -fr * sa
    theta[:, 1, 0] = fr * sa
    theta[:, 1, 1] = fr * ca
    theta[:, 0, 2] = 2 * u0 + f - 1
    theta[:, 1, 2] = 2 * v0 + f - 1
    S = args.size
    grid = F.affine_grid(theta, (B, 1, S, S), align_corners=False)
    fld = None
    if train and args.refract > 0:
        fld = refract(B, S, cc.device, args.refract, args.refract_cells)
        grid = grid + fld.permute(0, 2, 3, 1)
    img = F.grid_sample(cc, grid, align_corners=False)
    # labels turn by the inverse roll about the window centre
    cu = (u0 + f / 2)[:, None]
    cv = (v0 + f / 2)[:, None]
    du = mta[:, 0:4:2] - cu
    dv = mta[:, 1:4:2] - cv
    c1, s1, f1 = ca[:, None], sa[:, None], fr[:, None]
    wu = 0.5 + (du * c1 + dv * s1) / f1
    wv = 0.5 + (dv * c1 - du * s1) / f1
    # carry the labels through the identical resample, roll and bend
    wu, wv = carry_pts(cc, grid, mta[:, 0:4:2], mta[:, 1:4:2], S, wu, wv)
    # off-window eyes pin at the window edge; obscure carries truth
    eyes = torch.stack([wu[:, 0], wv[:, 0], wu[:, 1], wv[:, 1]], 1).clamp(0.0, 1.0)
    y = torch.cat([eyes, mta[:, 4:5] / f1, mta[:, 5:6]], 1)
    return img, y


def target_sheet(session_dir, cc, mt, vstats, tex, count=24, train=True):
    # exactly what target eats, with its OWN labels drawn on top —
    # if a cross is not on an eye, the fault is upstream of the net
    from PIL import Image, ImageDraw
    S = args.size
    n = min(count, cc.shape[0])
    sel = np.linspace(0, cc.shape[0] - 1, n).astype(int)
    xb, yb = target_batch(torch.tensor(cc[sel]).to(dev),
                          torch.tensor(mt[sel]).to(dev), train)
    xb = renorm_levels(xb, vstats)
    if train and args.scramble > 0:
        cx = (yb[:, 0] + yb[:, 2]) / 2 * S
        cy = (yb[:, 1] + yb[:, 3]) / 2 * S
        xb = scramble(xb, box_protect(cx, cy, yb[:, 4] * 0.75 * S), tex)
    if train:
        xb = sensor(xb)
    a = xb.clamp(0, 1).cpu().numpy()
    y = yb.cpu().numpy()
    Z, cells = 6, []
    for i in range(n):
        im = Image.fromarray((a[i, 0] * 255).astype(np.uint8))
        im = im.resize((S * Z, S * Z), Image.NEAREST).convert('RGB')
        d = ImageDraw.Draw(im)
        for (u, v), col in (((y[i, 0], y[i, 1]), (0, 255, 0)),
                            ((y[i, 2], y[i, 3]), (255, 0, 255))):
            px, py = float(u) * S * Z, float(v) * S * Z
            d.line([px - 7, py, px + 7, py], fill=col, width=2)
            d.line([px, py - 7, px, py + 7], fill=col, width=2)
        d.text((2, 2), f'ob{y[i, 5]:.2f} sc{y[i, 4]:.2f}', fill=(255, 90, 90))
        cells.append(im)
    across = 6
    rows = (n + across - 1) // across
    sh = Image.new('RGB', (across * S * Z, rows * S * Z), (24, 24, 24))
    for i, c in enumerate(cells):
        sh.paste(c, ((i % across) * S * Z, (i // across) * S * Z))
    nm = 'target-inputs.png' if train else 'target-clean.png'
    p = os.path.join(session_dir, nm)
    sh.save(p)
    print(f'target sheet {nm} ({n} samples, green=left magenta=right) -> {p}')


def train_target(tcc, tmt, lfid, lpup, rec, session_dir):
    tr, ev = split(lfid, lpup, 'target')
    # an eye far outside the context window stores a label that clamps
    # to 0 or 1 — no position in it. keep a small overshoot so obscure
    # still has near-edge examples, drop the rest from train AND eval.
    u = np.asarray(tmt)[:, 0:4]
    ok = np.where(((u > -0.2) & (u < 1.2)).all(1))[0]
    keep = np.zeros(len(tmt), bool)
    keep[ok] = True
    dropped = len(tmt) - keep.sum()
    tr = np.array([i for i in tr if keep[i]])
    ev = np.array([i for i in ev if keep[i]])
    print(f'target: dropped {dropped} views with off-window eye labels '
          f'-> {len(tr)} train / {len(ev)} eval')
    cc = torch.tensor(tcc)
    mt = torch.tensor(tmt)
    xev, yev = target_batch(cc[ev].to(dev), mt[ev].to(dev), False)
    tex = torch.tensor(build_texpool()).to(dev)
    S = args.size
    vstats = level_stats(rec['ctx']) if rec is not None else None
    target_sheet(session_dir, tcc, tmt, vstats, tex)
    target_sheet(session_dir, tcc, tmt, vstats, tex, train=False)

    def step(m):
        i = torch.randint(0, len(tr), (args.batch,))
        cb, mb = cc[tr][i].to(dev), mt[tr][i].to(dev)
        # every fetched crop yields tgt_rep independent windows
        if args.tgt_rep > 1:
            cb = cb.repeat(args.tgt_rep, 1, 1, 1)
            mb = mb.repeat(args.tgt_rep, 1)
        xb, yb = target_batch(cb, mb, True)
        xb = renorm_levels(xb, vstats)
        if args.scramble > 0:
            cx = (yb[:, 0] + yb[:, 2]) / 2 * S
            cy = (yb[:, 1] + yb[:, 3]) / 2 * S
            xb = scramble(xb, box_protect(cx, cy, yb[:, 4] * 0.75 * S), tex)
        xb = sensor(xb)
        return F.mse_loss(m(xb), yb)

    loop(PointNet(0.0, 1.0, 2), 'target',
         ['lx', 'ly', 'rx', 'ry', 'scale', 'obscure'],
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
        # whatever the cameras are named, take every frame of the pose
        frames += sorted(glob.glob(os.path.join(d, f'{fid}-*.png')))
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
            rsc = rec['gate_sc']
            dsc = np.abs(laux[idx][:, 8:9] - rsc[None, :]) / rsc[None, :]
            ms = (laux[idx][:, 0:2] + laux[idx][:, 2:4]) / 2
            mr = rec['gate_mid']
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
    # the real recordings, loaded as PAIRS: an annotated view feeds
    # find/target/look; its un-annotated sibling still shows the
    # person, and the rig disparity places the default hint plot on
    # it — real VALIDATION rows for the hint model
    from PIL import Image, ImageDraw
    rd = os.path.join(session_dir, 'record')
    if not os.path.isdir(rd):
        return None
    print(f'loading validation images from {rd} ...')
    S = args.size
    out = {'l': [], 'r': [], 'f': [], 'aux': [], 'plots': [], 'dist': [],
           'wf': [], 'find_y': [], 'ctx': [], 'target_y': [],
           'gate_mid': [], 'gate_sc': []}
    zc = lambda p: (p[0] - 0.5, p[1] - 0.5)
    recs = []
    for fn in sorted(os.listdir(rd)):
        if not fn.endswith('.agi'):
            continue
        text = open(os.path.join(rd, fn)).read()
        if 'noise: true' in text:
            continue
        fid = fn[:-4]
        cams = int(read_pair(text, 'cameras')[0])
        # role names come from the record's own images: block (tl/tr, or
        # top/bot on sets recorded before cameras had names) — never hardcoded
        roles = re.findall(r'^ {4}(\w+):\s*\S+\.png\s*$', text, re.M)
        roles = roles[:max(1, cams)]
        ann = {}
        for cs in roles:
            ocl = read_pair(text, f'{cs}_left_oc')
            ocr = read_pair(text, f'{cs}_right_oc')
            nb  = read_pair(text, f'{cs}_nose_base')
            pl  = read_pair(text, f'{cs}_left_pupil')
            pr  = read_pair(text, f'{cs}_right_pupil')
            if min(ocl[0], ocr[0], nb[0], pl[0], pr[0]) < -900:
                ann[cs] = None
                continue
            # record annotations are 0..1 frame fractions
            ocl, ocr, nb, pl, pr = zc(ocl), zc(ocr), zc(nb), zc(pl), zc(pr)
            sc = float(np.hypot(ocl[0] - ocr[0], ocl[1] - ocr[1]))
            ann[cs] = None if sc <= 0.02 else (ocl, ocr, nb, pl, pr, sc)
        recs.append((fid, text, roles, ann))
    # rig disparity, measured from the both-annotated pairs: the same
    # face sits a near-constant offset apart between the two views
    # (tl/tr shifts x, top/bot shifts y), so a lone annotation places
    # the sibling view's default hint plot
    dsum, dn = np.zeros(2), 0
    for fid, text, roles, ann in recs:
        if len(roles) > 1 and ann.get(roles[0]) and ann.get(roles[1]):
            a0, a1 = ann[roles[0]], ann[roles[1]]
            mid = lambda a: np.array([(a[0][0] + a[1][0]) / 2,
                                      (a[0][1] + a[1][1]) / 2])
            dsum += mid(a0) - mid(a1)
            dn += 1
    D = dsum / dn if dn else None
    if D is not None:
        print(f'pair disparity: x {D[0]:+.3f} y {D[1]:+.3f} '
              f'from {dn} both-view pairs')
    for fid, text, roles, ann in recs:
        look = read_pair(text, 'look')
        head = read_pair(text, 'head')
        # the station era is over: recordings carry no true_dist
        td = read_pair(text, 'true_dist')[0]
        if td < -900:
            td = 0.0
        for ri, cs in enumerate(roles):
            ip = os.path.join(rd, f'{fid}-{cs}.png')
            if not os.path.exists(ip):
                continue
            if ann.get(cs) is None:
                # un-annotated sibling: annotated view + disparity =
                # default plot on this frame's padded canvas, its
                # obscure graded from how far off-screen that sits
                sib = ann.get(roles[1 - ri]) if len(roles) > 1 else None
                if sib is None or D is None:
                    continue
                img = Image.open(ip).convert('L')
                gray = np.asarray(img, np.float32) / 255.0
                H, W = gray.shape
                smid = np.array([(sib[0][0] + sib[1][0]) / 2,
                                 (sib[0][1] + sib[1][1]) / 2])
                sgn = 1.0 if ri == 0 else -1.0
                hx, hy = smid + sgn * D
                wfp, _, _, _ = crop_gray(gray, W / 2, H / 2, W * PAD, S)
                sob = obscure_box(hx, hy, sib[5])
                out['wf'].append(wfp)
                out['find_y'].append(hint_plot(hx, hy, sib[5])
                                     + [sib[5] / PAD, sob])
                continue
            ocl, ocr, nb, pl, pr, sc = ann[cs]
            img = Image.open(ip).convert('L')
            gray = np.asarray(img, np.float32) / 255.0
            H, W = gray.shape
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
            # find scores on the padded canvas: the box-overlap plot,
            # scale vs the oc span, obscure vs the off-screen amount
            wfp, _, _, _ = crop_gray(gray, W / 2, H / 2, W * PAD, S)
            mfx = (ocl[0] + ocr[0]) * 0.5
            mfy = (ocl[1] + ocr[1]) * 0.5
            ob = obscure_box(mfx, mfy, sc)
            out['wf'].append(wfp)
            out['find_y'].append(hint_plot(mfx, mfy, sc) + [sc / PAD, ob])
            # donor gating stays sensor-frame: pupil midpoint + scale
            out['gate_mid'].append([(pl[0] + pr[0]) * 0.5,
                                    (pl[1] + pr[1]) * 0.5])
            out['gate_sc'].append(sc)
            # target scores on a 2x-scale window at the oc midpoint,
            # clamped onto the sensor like the trainer's hint crop
            mx, my = (ocl[0] + ocr[0]) / 2, (ocl[1] + ocr[1]) / 2
            tcs = sc * 2 * W
            tcx = (mx + 0.5) * W
            tcy = (my + 0.5) * H
            tcx = W / 2 if tcs >= W else min(max(tcx, tcs / 2), W - tcs / 2)
            tcy = H / 2 if tcs >= H else min(max(tcy, tcs / 2), H - tcs / 2)
            tc, tx0, ty0, ts2 = crop_gray(gray, tcx, tcy, tcs, S)
            def win_frac(p):
                return (((p[0] + 0.5) * W - tx0) / ts2,
                        ((p[1] + 0.5) * H - ty0) / ts2)
            wlu, wlv = win_frac(pl)
            wru, wrv = win_frac(pr)
            out['ctx'].append(tc)
            out['target_y'].append([wlu, wlv, wru, wrv, sc * W / ts2, ob])
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
    # real-frame score: plot vs true / disparity-default hints,
    # scale vs the oc span, obscure vs the off-screen amount
    x = torch.tensor(rec['wf']).to(dev)
    y = torch.tensor(rec['find_y']).to(dev)

    def fn(m):
        e = (m(x) - y).abs().mean(0)
        return (['v.x', 'v.y', 'v.scale', 'v.obscure'],
                [float(v) for v in e])
    return fn


def target_val(rec):
    # real-crop score: eyes + scale inside the 2x-scale window
    x = torch.tensor(rec['ctx']).to(dev)
    y = torch.tensor(rec['target_y']).to(dev)

    def fn(m):
        p = m(x)
        e = (p - y).abs().mean(0)
        return (['v.lx', 'v.ly', 'v.rx', 'v.ry', 'v.scale', 'v.obscure'],
                [float(v) for v in e])
    return fn


def look_val(rec):
    # distance is not validated here: it transfers in from the
    # synthetic labels the real pixels blend into
    t = lambda k: torch.tensor(rec[k]).to(dev)
    l, r, f2, a1 = t('l'), t('r'), t('f'), t('aux')
    plots = t('plots')
    n = len(rec['dist'])
    z3 = torch.zeros(n, 3, device=dev)
    # record rows are per view, so validation runs MONOCULAR on real
    # pixels: view 0 carries the record, view 1 is blind and says so.
    # it is a lower bound on the paired model, and diagnostic only
    zc = torch.zeros_like(l)
    obs = torch.zeros(n, 2, device=dev)
    obs[:, 1] = 1.0
    a2 = torch.cat([a1, torch.zeros(n, NAUX, device=dev), obs], 1)

    def fn(m):
        p, _ = m(l, r, f2, zc, zc, zc, a2, z3, z3, z3, z3)
        e = (p[:, :4] - plots).abs().mean(0)
        return (['v.head.x', 'v.head.y', 'v.gaze.x', 'v.gaze.y'],
                [float(v) for v in e])
    return fn


def slide_x(ab, yb, rg):
    # slide the head along rig x. a view's frame moves by the meters
    # divided by 2*dz*tan(fov/2) — the metres one frame width covers
    # at that camera's depth — so hcx, both screen plots and both
    # camera distances all follow exactly. tl/tr share dz, so one
    # slide moves both frames by the same amount
    B = ab.shape[0]
    hc = yb[:, 4:7]
    lo = torch.full((B, 1), -1e9, device=dev)
    hi = torch.full((B, 1), 1e9, device=dev)
    seen = []
    k = []
    for c in (0, 1):
        cp = rg[:, 2 + c * 5:5 + c * 5]
        tl = torch.deg2rad(rg[:, 5 + c * 5:6 + c * 5])
        hf = torch.deg2rad(rg[:, 6 + c * 5:7 + c * 5]) * 0.5
        d = hc - cp
        dz = (-d[:, 1:2] * torch.sin(tl)
              - d[:, 2:3] * torch.cos(tl)).clamp(min=1e-3)
        k.append(2.0 * dz * torch.tan(hf))
        a = ab[:, c * NAUX:(c + 1) * NAUX]
        mid = (a[:, 4:5] + a[:, 6:7]) * 0.5
        lim = (0.5 - a[:, 8:9] * 0.5).clamp(min=0.0)
        sn = ab[:, NAUX2 - 2 + c:NAUX2 - 1 + c] < 1.0
        seen.append(sn)
        # the face stays on every sensor that can see it
        lo = torch.where(sn, torch.maximum(lo, (-lim - mid) * k[c]), lo)
        hi = torch.where(sn, torch.minimum(hi, (lim - mid) * k[c]), hi)
    live = (seen[0] | seen[1]) & (hi > lo)
    dx = torch.where(live, lo + torch.rand(B, 1, device=dev) * (hi - lo),
                     torch.zeros(B, 1, device=dev)) * args.slide
    ab = ab.clone()
    for c in (0, 1):
        s = (dx / k[c] * seen[c].float())[:, 0]
        for j in (0, 2, 4, 6, 9, 12, 15):
            ab[:, c * NAUX + j] = ab[:, c * NAUX + j] + s
    yb = yb.clone()
    # he moved sideways and kept looking at the same spot: the screen
    # plots stay, hcx moves, the two distances follow from it
    yb[:, 4:5] = yb[:, 4:5] + dx
    for c in (0, 1):
        cp = rg[:, 2 + c * 5:5 + c * 5]
        yb[:, 7 + c:8 + c] = (yb[:, 4:7] - cp).norm(dim=1, keepdim=True)
    return ab, yb


def train_look(P, rec):
    # P is the PAIRED set: one row per pose carrying both views
    (tl, tr_, tf_, tl1, tr1, tf1, paux, ly, lfid, lgeo,
     ppl0, ppr0, ppl1, ppr1, prig) = P
    laux = paux[:, :NAUX]          # view 0 slice, for the donor gating below
    tr, ev = split(lfid, None, 'look')
    y8 = np.concatenate([ly, lgeo], 1).astype(np.float32)
    t = lambda a: torch.tensor(a)
    l, r, f2 = t(tl), t(tr_), t(tf_)
    l1, r1, f1 = t(tl1), t(tr1), t(tf1)
    a2, y2, g2 = t(paux), t(y8), t(prig)
    pl2, pr2 = t(ppl0.astype(np.float32)), t(ppr0.astype(np.float32))
    pl3, pr3 = t(ppl1.astype(np.float32)), t(ppr1.astype(np.float32))
    evd = [x[ev].to(dev) for x in (l, r, f2, l1, r1, f1, a2, pl2, pr2, pl3, pr3)]
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
        rsc = rec['gate_sc']
        dsc = np.abs(laux[:, 8:9] - rsc[None, :]) / rsc[None, :]
        # eyes median must also sit in the same part of the frame
        ms = (laux[:, 0:2] + laux[:, 2:4]) / 2
        mr = rec['gate_mid']
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
        lb, rb, fb, l1b, r1b, f1b, ab, plb, prb, pl1b, pr1b = [
            x[tr][i].to(dev)
            for x in (l, r, f2, l1, r1, f1, a2, pl2, pr2, pl3, pr3)]
        yb = y2[tr][i].to(dev)
        if args.slide > 0:
            ab, yb = slide_x(ab, yb, g2[tr][i].to(dev))
        # one validation sample sets levels, one blur for all 3
        pick = None
        if vl is not None:
            pick = torch.randint(0, vl.shape[0], (lb.shape[0],), device=dev)
        lb = renorm_levels(lb, vl, pick)
        rb = renorm_levels(rb, vr, pick)
        fb = renorm_levels(fb, vf, pick)
        l1b = renorm_levels(l1b, vl, pick)
        r1b = renorm_levels(r1b, vr, pick)
        f1b = renorm_levels(f1b, vf, pick)
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
            fb = scramble(fb, face_protect(ab[:, :NAUX]), tex)
            l1b = scramble(l1b, eyem, tex)
            r1b = scramble(r1b, eyem, tex)
            f1b = scramble(f1b, face_protect(ab[:, NAUX:2 * NAUX]), tex)
        lb = sensor(lb, pupil_keep())
        rb = sensor(rb, pupil_keep())
        fb = sensor(fb)
        l1b = sensor(l1b, pupil_keep())
        r1b = sensor(r1b, pupil_keep())
        f1b = sensor(f1b)
        if args.aux_noise > 0:
            # the two obscure factors are facts, never noised
            nz = torch.randn_like(ab) * args.aux_noise
            nz[:, -2:] = 0.0
            ab = ab + nz
        p, ploss = m(lb, rb, fb, l1b, r1b, f1b, ab, plb, prb, pl1b, pr1b)
        return F.mse_loss(p, yb) + ploss

    loop(LookNet(), 'look',
         ['head.x', 'head.y', 'gaze.x', 'gaze.y',
          'hcx', 'hcy', 'hcz', 'dist0', 'dist1'],
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
    loaded = [load_session(f'/src/hyperspace-sessions/{s}') for s in names]
    parts  = [a for a, _ in loaded]
    ppar   = [b for _, b in loaded]
    tl, tr_, tf_, tw = [np.concatenate([p[k] for p in parts]) for k in range(4)]
    laux = np.concatenate([p[4] for p in parts])
    ly   = np.concatenate([p[5] for p in parts])
    lfid = np.concatenate([p[6] + 1000000 * i for i, p in enumerate(parts)])
    lpup = np.concatenate([p[7] for p in parts])
    lgeo = np.concatenate([p[8] for p in parts])
    tcc  = np.concatenate([p[9] for p in parts])
    tmt  = np.concatenate([p[10] for p in parts])
    tsf  = np.concatenate([p[11] for p in parts])
    # paired look set, concatenated the same way
    P = [np.concatenate([q[k] for q in ppar]) for k in range(15)]
    P[8] = np.concatenate([q[8] + 1000000 * i for i, q in enumerate(ppar)])
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
        train_find(tsf, laux, lfid, rec)
    if args.process in ('all', 'target'):
        train_target(tcc, tmt, lfid, lpup, rec, sdir0)
    if args.process in ('all', 'look'):
        train_look(P, rec)


if __name__ == '__main__':
    main()
