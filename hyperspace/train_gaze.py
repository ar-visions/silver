# where on the screen he looks: the gaze runs' captures (both
# cameras, the dot's place), their eye points by the chain, then each
# eye's clip and the upper face cut by those points, into a net
import argparse
import glob
import os
import sys

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import train_eyes as base  # noqa: E402
from train import FaceNet, normalize, stage  # noqa: E402

STATE = '/src/silver/install/home/hyperspace/.local/state/hyperspace'
# a clip's side, the eye clip's width and the upper face's, in the
# outer corner gap
CLIP = 32
EYE_CLIP = 32
# the clip encoders' first conv kernel (5 sees an edge, as find's)
FIRST_K = 5
# the stored clips: this many times the span and the size (48 x 48),
# so training can shift and turn a 32 x 32 window inside them
WIDE = 1.5
# each capture's cameras judged sound (1) or not, set once loaded
SOUND = None
# the cameras the net reads (--camera: one of them)
CAMS = [0, 1]
# each capture's cameras' spots on the screen, in its -0.5..0.5 (y
# down) [n, 2, 2], set once loaded
SPOT = None
# an adult's outer eye corners apart, metres: a slew's pixels
EYE_GAP = 0.09
# a camera's 18 numbers: pupils, corners, the three boxes; x, y
X_COLS = (0, 2, 4, 6, 9, 12, 15)
Y_COLS = (1, 3, 5, 7, 10, 13, 16)
# a camera's numbers: pupils, outer corners, the gap, three boxes
NAUX = 18
EYE_SPAN = 1.25 / 3
# the eye's middle: this share of the corner gap in from its outer
# corner
EYE_IN = 0.17
FACE_SPAN = 1.15
# the upper face's middle: this share of the gap below the corner line
FACE_DROP = 0.1


def nets(run, dev):
    out = []
    for name, model in (('find', base.FindNet()),
                        ('target', FaceNet(base.SIDE)),
                        ('eye', base.EyeNet())):
        pt = os.path.join(run, f'{name}.pt')
        model.load_state_dict(torch.load(pt, map_location='cpu'))
        out.append(model.to(dev).eval())
    return out


# every capture: both frames, the chain's points on each, the dot
# one camera's answer judged on its own (the other camera may not
# see the face): 0 sound, else the first check it failed (1 shape,
# 2 unstable). p the points in pixels, again the eye stage rerun
def judge(p, again, w):
    lo, ro, lp, rp = p[0:2], p[2:4], p[4:6], p[6:8]
    d = ro - lo
    sc = np.hypot(*d)
    if sc < 0.06 * w or sc > 0.6 * w:
        return 1
    u = d / sc
    n = np.array([-u[1], u[0]])
    la, ra = (lp - lo) @ u / sc, (rp - ro) @ u / sc
    lb, rb = (lp - lo) @ n / sc, (rp - ro) @ n / sc
    # each pupil just inside its outer corner, near the corner line;
    # the face no more than 40 degrees off level
    if not (0.08 < la < 0.45 and -0.45 < ra < -0.08):
        return 1
    if abs(lb) > 0.15 or abs(rb) > 0.15 or abs(u[1]) > 0.64:
        return 1
    # the eye stage again on its own answer: a real eye stays put
    if np.abs(again - p).max() > 0.06 * sc:
        return 2
    return 0


def records(run, dev):
    find, target, eye = nets(run, dev)
    frames, points, dots, sess, marks = [], [], [], [], []
    spots = []
    kv = {}
    for f in sorted(glob.glob(os.path.join(STATE, 'gaze', '*.agi'))):
        stem = f[:-4]
        kv = dict(t.split('=', 1) for t in open(f).read().split()
                  if '=' in t)
        fs, ps, ms = [], [], []
        for cam in (0, 1):
            pic = f'{stem}-camera{cam}.png'
            if not os.path.exists(pic):
                break
            g = Image.open(pic).convert('L')
            a = np.asarray(g, np.float32) / 255
            full = torch.tensor(a)[None, None]
            p, _ = base.run_pair(find, target, full, dev)
            p = base.run_eyes(eye, full, p, dev)
            again = base.run_eyes(eye, full, p, dev)
            ms.append(judge(np.asarray(p), np.asarray(again),
                            a.shape[1]))
            ps.append(p)
            fs.append(np.asarray(g, np.uint8))
        if len(fs) < 2:
            continue
        frames.append(np.stack(fs))
        points.append(np.stack(ps).astype(np.float32))
        marks.append(ms)
        # the gaze: on the screen, its middle 0, its edges -0.5, 0.5
        dots.append([float(kv['target_x']) - 0.5,
                     float(kv['target_y']) - 0.5])
        sess.append(kv.get('session', '0'))
        # each camera's spot: metres from the screen's middle (y up)
        # over the screen's size
        sw = float(kv['screen_width_m'])
        sh = float(kv['screen_height_m'])
        spots.append([[float(kv[f'camera_x_m{c}']) / sw,
                       -float(kv[f'camera_y_m{c}']) / sh]
                      for c in (0, 1)])
    if not frames:
        sys.exit('no gaze captures: run hyperspace\'s gaze entry')
    marks = np.asarray(marks)
    for cam in (0, 1):
        m = marks[:, cam]
        print(f'camera {cam + 1}: {(m == 0).mean():.0%} sound, '
              f'{(m == 1).mean():.0%} bad shape, '
              f'{(m == 2).mean():.0%} unstable', flush=True)
    return (np.stack(frames), np.stack(points),
            np.asarray(dots, np.float32), np.asarray(sess), kv, marks,
            np.asarray(spots, np.float32))


# clips from frames [b, 1, h, w] at centres (px), sides (px) and
# angles; mirrored ones flipped left to right
def clips(frames, cx, cy, side, ang, mirror, size):
    b, _, h, w = frames.shape
    c, s = torch.cos(ang), torch.sin(ang)
    th = torch.zeros(b, 2, 3, device=frames.device)
    th[:, 0, 0] = side * c / w
    th[:, 0, 1] = -side * s / w
    th[:, 1, 0] = side * s / h
    th[:, 1, 1] = side * c / h
    th[:, 0, 2] = cx / w * 2 - 1
    th[:, 1, 2] = cy / h * 2 - 1
    g = F.affine_grid(th, (b, 1, size, size), align_corners=False)
    out = F.grid_sample(frames, g, align_corners=False)
    return torch.where(mirror.view(-1, 1, 1, 1), out.flip(3), out)


# a camera's three clips [b, 3, CLIP, CLIP] (left eye, right eye
# mirrored, the upper face) and its 18 numbers as hyperspace2 had
# them: pupils, outer corners, the gap, each clip's box, all in
# shares of the frame; jitter shifts, sizes and turns the clips
def camera_clips(frames, p, jitter, sound, wide=1.0):
    b, dev = frames.shape[0], frames.device
    h, w = frames.shape[2], frames.shape[3]

    def u(lo, hi):
        if not jitter:
            return torch.zeros(b, device=dev)
        return torch.empty(b, device=dev).uniform_(lo, hi)

    dx, dy = p[:, 2] - p[:, 0], p[:, 3] - p[:, 1]
    sc = torch.sqrt(dx * dx + dy * dy).clamp(min=1.0)
    ang = torch.atan2(dy, dx) + u(-0.08, 0.08)
    grow = 1 + u(-0.08, 0.08)
    out, boxes = [], []

    def box(x, y, side):
        boxes.append(torch.stack([x / w - 0.5, y / h - 0.5,
                                  side / w], 1))

    for k, mir in ((0, False), (1, True)):
        # on the eye, not the pupil: from the outer corner along the
        # corner line, so the pupil moves in the clip as the gaze does
        way = 1.0 if k == 0 else -1.0
        ex = p[:, 2 * k] + way * dx * EYE_IN + sc * u(-0.03, 0.03)
        ey = p[:, 2 * k + 1] + way * dy * EYE_IN + sc * u(-0.03, 0.03)
        side = sc * EYE_SPAN * grow * wide
        m = torch.full((b,), mir, device=dev)
        out.append(clips(frames, ex, ey, side, ang, m,
                         round(EYE_CLIP * wide)))
        box(ex, ey, side)
    mx = (p[:, 0] + p[:, 2]) / 2 - dy * FACE_DROP + sc * u(-0.04, 0.04)
    my = (p[:, 1] + p[:, 3]) / 2 + dx * FACE_DROP + sc * u(-0.04, 0.04)
    side = sc * FACE_SPAN * grow * wide
    m = torch.zeros(b, dtype=torch.bool, device=dev)
    face = clips(frames, mx, my, side, ang, m, round(CLIP * wide))
    box(mx, my, side)
    # pupils then outer corners, as hyperspace2's order
    xs, ys = p[:, 0::2] / w, p[:, 1::2] / h
    at = torch.stack([xs[:, 2], ys[:, 2], xs[:, 3], ys[:, 3],
                      xs[:, 0], ys[:, 0], xs[:, 1], ys[:, 1]], 1) - 0.5
    aux = torch.cat([at, (sc / w)[:, None]] + boxes, 1)
    eyes = torch.cat(out, 1)
    # seen: the detection check's word on this camera (sound or
    # not); a flag the net reads, nothing zeroed
    seen = sound
    return eyes, face, aux, seen


# each clip to 64 numbers (the four eyes one encoder, the faces
# another), then those, both cameras' numbers and flags to the dot
class GazeNet(nn.Module):
    def __init__(self, cams):
        super().__init__()
        self.cams = list(cams)
        n = len(self.cams)

        def enc():
            return nn.Sequential(*stage(1, 16, FIRST_K), *stage(16, 32),
                                 *stage(32, 64), nn.Flatten(),
                                 nn.Linear(64 * 4 * 4, 64))
        self.eye = enc()
        self.face = enc()
        # its input scaled first: unscaled, the first step blew it up
        width = 3 * n * 64 + n * NAUX + n
        self.head = nn.Sequential(
            nn.LayerNorm(width), nn.Linear(width, 256), nn.ReLU(),
            nn.Dropout(0.2), nn.Linear(256, 2))

    def forward(self, eyes, faces, aux):
        b, n = eyes.shape[0], len(self.cams)
        e = eyes.reshape(b * 2 * n, 1, EYE_CLIP, EYE_CLIP)
        fa = faces.reshape(b * n, 1, CLIP, CLIP)
        e = self.eye(normalize(e)).view(b, -1)
        fa = self.face(normalize(fa)).view(b, -1)
        return self.head(torch.cat([e, fa, aux], 1))


# every capture's clips cut once, WIDE times the span and size, as
# bytes on the GPU (frames never go there): eyes [n, 4], faces
# [n, 2], numbers and seen flags [n, 38]
def clip_bank(fr, pt, dev):
    eyes, faces, nums = [], [], []
    for k in range(0, len(fr), 256):
        f = torch.tensor(fr[k:k + 256], device=dev).float() / 255
        p = torch.tensor(pt[k:k + 256], device=dev)
        cams = [camera_clips(f[:, c:c + 1], p[:, c], False,
                             SOUND[k:k + 256, c], WIDE)
                for c in (0, 1)]
        eyes.append(torch.cat([q[0] for q in cams], 1).mul(255)
                    .round().to(torch.uint8))
        faces.append(torch.cat([q[1] for q in cams], 1).mul(255)
                     .round().to(torch.uint8))
        nums.append(torch.cat([q[2] for q in cams]
                              + [torch.stack([q[3] for q in cams], 1)],
                              1))
    return torch.cat(eyes), torch.cat(faces), torch.cat(nums)


# a 32 x 32 window of each stored clip: the middle 1 / WIDE of it,
# or (training) shifted, turned and sized a little; each clip's box
# in the numbers moved with its window. clip slots: (camera, which)
# with which 0 left eye, 1 right eye (mirrored), 2 face
def window(c, size, jitter, slots, nums):
    b, n = c.shape[0], c.shape[1]
    dev = c.device

    def u(lo, hi):
        if not jitter:
            return torch.zeros(b, n, device=dev)
        return torch.empty(b, n, device=dev).uniform_(lo, hi)

    scale = (1 + u(-0.08, 0.08)) / WIDE
    turn = u(-0.08, 0.08)
    tx, ty = u(-0.06, 0.06), u(-0.06, 0.06)
    th = torch.zeros(b * n, 2, 3, device=dev)
    co, si = torch.cos(turn).flatten(), torch.sin(turn).flatten()
    sf = scale.flatten()
    th[:, 0, 0], th[:, 0, 1] = sf * co, -sf * si
    th[:, 1, 0], th[:, 1, 1] = sf * si, sf * co
    th[:, 0, 2], th[:, 1, 2] = tx.flatten(), ty.flatten()
    g = F.affine_grid(th, (b * n, 1, size, size), align_corners=False)
    flat = c.reshape(b * n, 1, c.shape[2], c.shape[3])
    out = F.grid_sample(flat, g, align_corners=False)
    for j, (cam, which) in enumerate(slots):
        at = cam * NAUX + 9 + 3 * which
        side = nums[:, at + 2].clone()
        way = -1.0 if which == 1 else 1.0
        nums[:, at] = nums[:, at] + way * tx[:, j] / 2 * side
        nums[:, at + 1] = nums[:, at + 1] + ty[:, j] / 2 * side
        nums[:, at + 2] = side * scale[:, j]
    return out.view(b, n, size, size)


EYE_SLOTS = [(0, 0), (0, 1), (1, 0), (1, 1)]
FACE_SLOTS = [(0, 2), (1, 2)]


# the numbers' columns of the cameras read: each one's 18, then
# each one's seen flag
def aux_cols():
    return ([c * NAUX + i for c in CAMS for i in range(NAUX)]
            + [2 * NAUX + c for c in CAMS])


# a batch from the clip bank: per camera read, 2 eye clips, a face
# clip, 18 numbers and its seen flag
def batch(bank, idx, jitter):
    eyes = bank[0][idx].float() / 255
    faces = bank[1][idx].float() / 255
    nums = bank[2][idx].clone()
    eyes = window(eyes, EYE_CLIP, jitter, EYE_SLOTS, nums)
    faces = window(faces, CLIP, jitter, FACE_SLOTS, nums)
    ei = [2 * c + k for c in CAMS for k in (0, 1)]
    return eyes[:, ei], faces[:, CAMS], nums[:, aux_cols()]


# the head moved by up to reach metres across and up or down, the
# eyes seen the same: every point on each camera moved by that
# camera's own pixels (its corner gap over EYE_GAP). Where the eyes
# look on the screen is the eye's look to the camera and its
# distance, not the head's place, so the dot stays
def slew(aux, reach):
    if reach <= 0:
        return aux
    b = aux.shape[0]
    dx = torch.empty(b, device=aux.device).uniform_(-reach, reach)
    dy = torch.empty(b, device=aux.device).uniform_(-reach, reach)
    out = aux.clone()
    for j in range(len(CAMS)):
        at = j * NAUX
        px = aux[:, at + 8] / EYE_GAP
        for c in X_COLS:
            out[:, at + c] = aux[:, at + c] + dx * px
        for c in Y_COLS:
            out[:, at + c] = aux[:, at + c] + dy * px
    return out


# the head slid along the camera's line of sight by a factor k (up
# to reach either way), the eyes seen the same: the gaze point moves
# k times as far from the camera's spot; every number the camera
# gives (points from the frame's middle, the gap, the boxes) shrinks
# by 1 / k. One camera only: two would see the move differently
def depth(aux, dots, take, reach):
    if reach <= 0 or len(CAMS) != 1:
        return aux, dots
    b = aux.shape[0]
    k = torch.exp(torch.empty(b, device=aux.device)
                  .uniform_(-np.log1p(reach), np.log1p(reach)))
    out = aux.clone()
    out[:, :NAUX] = aux[:, :NAUX] / k[:, None]
    spot = SPOT[take, CAMS[0]]
    return out, spot + k[:, None] * (dots - spot)


# every 1000 steps the errors, the best test step's net kept
def fit(bank, dots, tr, args, dev, err, te=None):
    torch.manual_seed(5)
    net = GazeNet(CAMS).to(dev)
    opt = torch.optim.AdamW(net.parameters(), lr=3e-4,
                            weight_decay=1e-2)
    ease = torch.optim.lr_scheduler.CosineAnnealingLR(opt, args.steps)
    idx = torch.tensor(np.nonzero(tr)[0], device=dev)
    ti = (torch.tensor(np.nonzero(te)[0], device=dev)
          if te is not None else None)
    best, kept = None, None
    for step in range(args.steps):
        net.train()
        take = idx[torch.randint(len(idx), (64,), device=dev)]
        eyes, faces, aux = batch(bank, take, True)
        aux, goal = depth(aux, dots[take], take, args.depth)
        out = net(shake(eyes), shake(faces), slew(aux, args.slew))
        loss = F.smooth_l1_loss(out, goal, beta=0.02)
        opt.zero_grad()
        loss.backward()
        nn.utils.clip_grad_norm_(net.parameters(), 1.0)
        opt.step()
        ease.step()
        if (step + 1) % 1000 == 0:
            some = idx[torch.randperm(len(idx), device=dev)[:512]]
            line = f'  step {step + 1}: training ' \
                f'{say(net, bank, dots, some, err)[0]}'
            if ti is not None:
                text, score = say(net, bank, dots, ti, err)
                line += f' | never trained on {text}'
                if best is None or score < best:
                    best = score
                    kept = {k: v.detach().clone()
                            for k, v in net.state_dict().items()}
            print(line, flush=True)
    if kept is not None:
        net.load_state_dict(kept)
    return net.eval()


# brightness, contrast and noise, every clip its own: no two views
# of a frame alike
def shake(c):
    b, n = c.shape[0], c.shape[1]
    dev = c.device

    def u(lo, hi):
        return torch.empty(b, n, 1, 1, device=dev).uniform_(lo, hi)

    mid = c.mean((2, 3), keepdim=True)
    c = (c - mid) * u(0.8, 1.2) + mid * u(0.85, 1.15)
    c = c + torch.randn_like(c) * u(0.0, 0.02)
    return c.clamp(0, 1)


def say(net, bank, dots, idx, err):
    e = err(run(net, bank, idx).cpu().numpy(),
           dots[idx].cpu().numpy())
    return f'mean {e.mean():.3f}, median {np.median(e):.3f}', e.mean()


@torch.no_grad()
def run(net, bank, idx):
    was = net.training
    net.eval()
    # in pieces: every capture at once does not fit the GPU
    out = torch.cat([net(*batch(bank, idx[k:k + 512], False))
                     for k in range(0, len(idx), 512)])
    net.train(was)
    return out


# a ridge fit on the 36 numbers alone: the plain baseline
def ridge(x, y, xt, lam=10.0):
    mu, sd = x.mean(0), x.std(0) + 1e-6
    z = np.c_[(x - mu) / sd, np.ones(len(x))]
    reg = lam * np.eye(z.shape[1])
    reg[-1, -1] = 0
    w = np.linalg.solve(z.T @ z + reg, z.T @ y)
    return np.c_[(xt - mu) / sd, np.ones(len(xt))] @ w


# up to 24 rejected camera frames with their points: what was thrown
def rejected_sheet(fr, pt, marks):
    from PIL import ImageDraw
    bad = np.argwhere(marks > 0)
    if len(bad) == 0:
        return
    many = min(24, len(bad))
    pick = bad[np.linspace(0, len(bad) - 1, many).astype(int)]
    side = 170
    sheet = Image.new('RGB', (6 * side, 4 * side), (20, 20, 24))
    for n, (i, cam) in enumerate(pick):
        g = Image.fromarray(fr[i, cam])
        g = g.resize((side, side)).convert('RGB')
        d = ImageDraw.Draw(g)
        p = pt[i, cam] * side / fr.shape[3]
        for j in range(4):
            col = (255, 70, 70) if j < 2 else (70, 255, 110)
            d.ellipse([p[2 * j] - 3, p[2 * j + 1] - 3,
                       p[2 * j] + 3, p[2 * j + 1] + 3], outline=col)
        why = 'shape' if marks[i, cam] == 1 else 'unstable'
        d.text((4, 4), f'cam {cam + 1} {why}', fill=(255, 210, 60))
        sheet.paste(g, ((n % 6) * side, (n // 6) * side))
    out = os.path.expanduser('~/Pictures/hyperspace/gaze-rejected.png')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    sheet.save(out)
    print(f'rejected frames: {out}', flush=True)


def main():
    p = argparse.ArgumentParser()
    runs_dir = os.path.expanduser('~/.cache/hyperspace/runs')
    p.add_argument('--run', default=os.path.join(runs_dir, 'eyes'))
    p.add_argument('--steps', type=int, default=30000)
    # trained and tested on the same n captures: the pipeline's check
    p.add_argument('--fit', type=int, default=0)
    p.add_argument('--across', action='store_true')
    # 1 or 2: that camera alone (mounts.agi's order)
    p.add_argument('--camera', type=int, default=0)
    # the head's slew in training, metres each way, across and up
    # or down (0 none)
    p.add_argument('--slew', type=float, default=0.1)
    # the head's distance in training: up to this share nearer or
    # farther (one camera only; 0 none)
    p.add_argument('--depth', type=float, default=0.15)
    a = p.parse_args()
    dev = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    global SOUND, CAMS
    if a.camera:
        CAMS = [a.camera - 1]
    fr, pt, dots, sess, kv, marks, spots = records(a.run, dev)
    global SPOT
    SPOT = torch.tensor(spots, device=dev)
    SOUND = torch.tensor((marks == 0).astype(np.float32), device=dev)
    rejected_sheet(fr, pt, marks)
    runs = sorted(set(sess), key=lambda s: -(sess == s).sum())
    print(f'{len(fr)} captures, runs of '
          + ', '.join(f'{(sess == s).sum()}' for s in runs)
          + '; errors in screen units, 0.5 half the screen',
          flush=True)
    bank = clip_bank(fr, pt, dev)
    dt = torch.tensor(dots, device=dev)
    seen_all = bank[2][:, -2:]
    feats = bank[2][:, aux_cols()].cpu().numpy()
    print(f'camera seen: camera 1 '
          f'{seen_all[:, 0].mean().item():.0%}, camera 2 '
          f'{seen_all[:, 1].mean().item():.0%}', flush=True)

    # an error in screen units: the marker's -0.5..0.5 space
    def err(pred, truth):
        d = pred - truth
        return np.hypot(d[:, 0], d[:, 1])

    if a.fit:
        some = np.zeros(len(fr), bool)
        some[np.linspace(0, len(fr) - 1, a.fit).astype(int)] = True
        print(f'the check: {a.fit} captures, '
              'trained and tested on them')
        fit(bank, dt, some, a, dev, err, some)
        return
    # 7-second pieces (one dot move each, 15 frames a second) of
    # each run, every 5th never trained on; --across: train on the
    # biggest run, check on the others
    te = np.zeros(len(fr), bool)
    for s in runs:
        at = np.nonzero(sess == s)[0]
        te[at] = (np.arange(len(at)) // 105) % 5 == 4
    if a.across and len(runs) > 1:
        te = sess != runs[0]
    tr = ~te
    print(f'training on {tr.sum()} captures, never trained on '
          f'{te.sum()}: {a.steps} steps of 64, a line every 1000',
          flush=True)
    net = fit(bank, dt, tr, a, dev, err, te)
    ti = torch.tensor(np.nonzero(te)[0], device=dev)
    e = err(run(net, bank, ti).cpu().numpy(), dots[te])
    r = err(ridge(feats[tr], dots[tr], feats[te]), dots[te])
    mid = err(np.tile(dots[tr].mean(0), (te.sum(), 1)), dots[te])
    print(f'never trained on ({te.sum()}): mean {e.mean():.3f}, '
          f'median {np.median(e):.3f}, worst {e.max():.3f} | '
          f'points alone {r.mean():.3f} | '
          f'the middle always {mid.mean():.3f}', flush=True)
    # calibrated: a straight-line fix (x and y each from both) fitted
    # on that recording's first fifth, scored on the rest of it
    pred = run(net, bank, ti).cpu().numpy()
    first = np.arange(len(pred)) < len(pred) // 5
    z = np.c_[pred, np.ones(len(pred))]
    fix = np.linalg.lstsq(z[first], dots[te][first], rcond=None)[0]
    c = err(z[~first] @ fix, dots[te][~first])
    print(f'calibrated on its first fifth, the rest: '
          f'mean {c.mean():.3f}, median {np.median(c):.3f}',
          flush=True)
    out = os.path.join(STATE, 'models', 'gaze.pt')
    torch.save(net.cpu().state_dict(), out)
    # and as the app reads it (hyperspace.c's hs_gaze_run)
    from export_eyes import write_gaze
    net.eval()
    write_gaze(net, os.path.join(STATE, 'models', 'gaze.hsg'))
    print(f'saved: {out}, and gaze.hsg beside it')


if __name__ == '__main__':
    main()
