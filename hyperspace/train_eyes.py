#!/usr/bin/env python3
# hyperspace's base targeting models: find (the face's square in
# the frame) and target (in that square: both outer eye corners,
# the scale, both eye centers), from Microsoft's FaceSynthetics:
# rendered faces at every angle, the pupils labelled. Half of each
# batch is steeply turned faces, a quarter frontal; every frame is
# rolled, placed and made to look like the IR cameras
import argparse
import io
import os
import sys
import urllib.request
import zipfile
from multiprocessing import Pool

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from train import FaceNet, StageConv, normalize  # noqa: E402

FRAME = 256
FIND = 64
SIDE = 64
# the eye stage's crop, and the face it is cut from (finer than
# target's, so the eye keeps its detail)
EYE = 64
FACE = 128
# the eye crop's width in sc, and its turn off level in training
EYE_SPAN = 0.7
EYE_TURN = 6.0 * np.pi / 180.0
HELD = 0.05
# each crop turned up to 15 degrees either way
TURN = 15.0 * np.pi / 180.0
# the face bent by at most this share of the crop's half width
BEND = 0.3


HANCO = 'https://lmb.informatik.uni-freiburg.de/data/HanCo'
HAND = 128


# real hands and forearms: HanCo's sample (green-screen frames
# with their masks), fetched once; each a grey cutout and its mask
# at HAND
def hand_bank(cache):
    path = os.path.join(cache, f'hands-{HAND}.npz')
    if os.path.exists(path):
        d = np.load(path)
        return d['hands'], d['masks']
    root = os.path.join(cache, 'hanco')
    zp = os.path.join(root, 'tester.zip')
    os.makedirs(root, exist_ok=True)
    if not os.path.exists(zp):
        urllib.request.urlretrieve(f'{HANCO}/HanCo_tester.zip', zp)
    hands, masks = [], []
    with zipfile.ZipFile(zp) as z:
        names = set(z.namelist())
        for n in sorted(names):
            if not n.startswith('rgb/') or not n.endswith('.jpg'):
                continue
            m = 'mask_fg/' + n[4:]
            if m not in names:
                continue
            im = Image.open(io.BytesIO(z.read(n))).convert('L')
            mk = Image.open(io.BytesIO(z.read(m))).convert('L')
            hands.append(np.asarray(im.resize(
                (HAND, HAND), Image.Resampling.BILINEAR), np.uint8))
            masks.append(np.asarray(mk.resize(
                (HAND, HAND), Image.Resampling.BILINEAR), np.uint8))
    hands, masks = np.stack(hands), np.stack(masks)
    np.savez(path, hands=hands, masks=masks)
    return hands, masks


# the user's own camera frames of his hands and hair (my*/cam*),
# whole frames at HAND x HAND; random sections of them cover faces
def own_bank(cache, which='my*'):
    import glob
    fs = sorted(glob.glob(os.path.join(cache, which, 'cam*',
                                       '*.png')))
    return np.stack([np.asarray(Image.open(f).convert('L').resize(
        (HAND, HAND), Image.Resampling.BILINEAR), np.uint8)
        for f in fs]) if fs else None


# hair cut from FaceSynthetics faces by their part maps (part 13):
# grey and mask at HAIR, with that face's eye middle and sc in
# shares of its picture, to lay on other faces by their eyes
HAIR = 128
HAIRS = 3000


def hair_one(item):
    img, seg, mark = item
    s = np.asarray(Image.open(seg))
    hm = (s == 13).astype(np.uint8) * 255
    if (hm > 0).mean() < 0.02:
        return None
    a = Image.open(img).convert('L')
    w, h = a.size
    p = np.loadtxt(mark) / np.array([w, h])
    mid = (p[36] + p[45]) / 2
    sc = np.linalg.norm(p[36] - p[45])
    grey = np.asarray(a.resize((HAIR, HAIR), Image.Resampling.BILINEAR))
    mask = np.asarray(Image.fromarray(hm).resize(
        (HAIR, HAIR), Image.Resampling.BILINEAR))
    return grey, mask, [mid[0], mid[1], sc]


def hair_bank(root, cache):
    path = os.path.join(cache, f'hair-{HAIR}.npz')
    if os.path.exists(path):
        d = np.load(path)
        return d['hair'], d['masks'], d['eyes']
    items = []
    for base, _, files in os.walk(root):
        for f in sorted(files):
            if f.endswith('_seg.png'):
                stem = f[:-len('_seg.png')]
                items.append((os.path.join(base, f'{stem}.png'),
                              os.path.join(base, f),
                              os.path.join(base, f'{stem}_ldmks.txt')))
    items.sort()
    with Pool(16) as pool:
        got = [g for g in pool.map(hair_one, items[:HAIRS * 2],
                                   chunksize=32) if g is not None]
    got = got[:HAIRS]
    hair = np.stack([g[0] for g in got])
    masks = np.stack([g[1] for g in got])
    eyes = np.asarray([g[2] for g in got], np.float32)
    np.savez(path, hair=hair, masks=masks, eyes=eyes)
    return hair, masks, eyes


def options():
    p = argparse.ArgumentParser()
    cache = os.path.expanduser('~/.cache/hyperspace')
    p.add_argument('--faces', default=os.path.join(cache,
                                                   'facesynth.zip'))
    p.add_argument('--out', default=os.path.join(cache, 'runs/eyes'))
    p.add_argument('--epochs', type=int, default=40)
    # each face's random crops an epoch
    p.add_argument('--views', type=int, default=1)
    # the share of the faces used (the first ones)
    p.add_argument('--share', type=float, default=1.0)
    # pictures an epoch, the same number from each pose bin
    p.add_argument('--epoch_size', type=int, default=20000)
    p.add_argument('--batch', type=int, default=128)
    return p.parse_args()


# the zip (or its four parts, joined) unpacked once beside it
def unpack(archive):
    root = os.path.join(os.path.dirname(archive), 'facesynth')
    done = os.path.join(root, '.unpacked')
    if os.path.exists(done):
        return root
    if not os.path.exists(archive):
        parts = [f'{archive[:-4]}.{n:03d}' for n in range(1, 5)]
        with open(archive, 'wb') as out:
            for part in parts:
                with open(part, 'rb') as f:
                    while True:
                        b = f.read(1 << 24)
                        if not b:
                            break
                        out.write(b)
    with zipfile.ZipFile(archive) as z:
        z.extractall(root)
    open(done, 'w').close()
    return root


# one face grey at FRAME, and its labels as shares of the picture:
# the outer eye corners, each eye's pupil (by the nearer eye), the
# face's square (the box of its 68 points, a quarter more), and how
# far it is turned
def decode(item):
    image_path, mark_path = item
    p = np.loadtxt(mark_path)
    image = Image.open(image_path).convert('L')
    w, h = image.size
    a = np.asarray(image.resize((FRAME, FRAME),
                                Image.Resampling.BILINEAR), np.uint8)
    p = p / np.array([w, h])
    eye_a = (p[36] + p[39]) / 2
    eye_b = (p[42] + p[45]) / 2
    pa, pb = p[68], p[69]
    if (np.linalg.norm(pa - eye_a) + np.linalg.norm(pb - eye_b)
            > np.linalg.norm(pa - eye_b) + np.linalg.norm(pb - eye_a)):
        pa, pb = pb, pa
    lo, hi = p[:68].min(0), p[:68].max(0)
    mid = (lo + hi) / 2
    side = (hi - lo).max() * 1.25
    pt = [*p[36], *p[45], *pa, *pb, *mid, side]
    # turned: the nose off the eyes' middle (left-right) and from
    # its usual place between eyes and mouth (up-down)
    eyes = (p[36] + p[45]) / 2
    mouth = (p[48] + p[54]) / 2
    span = max(np.linalg.norm(p[36] - p[45]),
               np.linalg.norm(eyes - mouth), 1e-4)
    yaw = abs(p[30][0] - eyes[0]) / span
    down = (p[30][1] - eyes[1]) / max(mouth[1] - eyes[1], 1e-4)
    turn = max(yaw / 0.35, abs(down - 0.62) / 0.25)
    return a, pt, turn


# the face's size: SPAN times sc (outer eye corner to outer eye
# corner), never under LEAST of the photo (a turned face's sc is small)
SPAN = 2.3
# pose bins a side: up-down by left-right
BINS = 6
# the share of pictures painted on a black canvas, the rest whole photos
CANVAS = 0.8
# the share of faces blown out by the lamp
BLOW = 0.8
# the share of faces given the rough chain; the rest stay normal
ROUGH = 0.5
# the learning rate at the top of its cycle
LR = 2e-2
# the heat map's own loss beside the point's (was 0.01)
HEAT_WEIGHT = 0.5
# his marked frames' share of every epoch
OWN_SHARE = 0.1
# the rows looking up and down most: each of their bins this many
# shares of an epoch, every other bin one
PITCH_WEIGHT = 10
LEAST = 0.3


def sized(pts):
    sc = np.hypot(pts[:, 0] - pts[:, 2], pts[:, 1] - pts[:, 3])
    pts = pts.copy()
    pts[:, 10] = np.maximum(sc * SPAN, LEAST)
    return pts


# each face's pose: up and down, the nose tip's place from the eyes
# (0) to the mouth (1), a straight face about 0.5, one seen from
# below near 0; left and right, the nose tip off the eyes' middle in
# sc. In load's order, kept beside its cache
def pose_one(path):
    p = np.loadtxt(path)
    eyes = (p[36] + p[45]) / 2
    mouth = (p[48] + p[54]) / 2
    sc = max(np.linalg.norm(p[36] - p[45]), 1e-4)
    return [(p[30][1] - eyes[1]) / max(mouth[1] - eyes[1], 1e-4),
            (p[30][0] - eyes[0]) / sc]


def poses(root):
    cache = os.path.join(os.path.dirname(root), 'pose.npy')
    if os.path.exists(cache):
        return np.load(cache)
    items = []
    for base, _, files in os.walk(root):
        for f in sorted(files):
            if f.endswith('_ldmks.txt'):
                stem = f[:-len('_ldmks.txt')]
                img = os.path.join(base, f'{stem}.png')
                if os.path.exists(img):
                    items.append((img, os.path.join(base, f)))
    items.sort()
    with Pool(16) as pool:
        t = np.asarray(pool.map(pose_one, [m for _, m in items],
                                chunksize=256), np.float32)
    np.save(cache, t)
    return t


def load(root):
    cache = os.path.join(os.path.dirname(root), f'synth-{FRAME}.npz')
    if os.path.exists(cache):
        d = np.load(cache)
        return d['frames'], sized(d['pts']), d['turns']
    items = []
    for base, _, files in os.walk(root):
        for f in sorted(files):
            if f.endswith('_ldmks.txt'):
                stem = f[:-len('_ldmks.txt')]
                img = os.path.join(base, f'{stem}.png')
                if os.path.exists(img):
                    items.append((img, os.path.join(base, f)))
    items.sort()
    with Pool(16) as pool:
        got = pool.map(decode, items, chunksize=64)
    frames = np.stack([g[0] for g in got])
    pts = np.asarray([g[1] for g in got], np.float32)
    turns = np.asarray([g[2] for g in got], np.float32)
    np.savez(cache, frames=frames, pts=pts, turns=turns)
    return frames, sized(pts), turns


# where the window runs past the picture: a background of smooth
# blotches and grain at a random level, new each time
def backdrop(x, valid):
    b, _, h, w = x.shape
    dev = x.device
    lv = torch.empty(b, 1, 1, 1, device=dev).uniform_(0.0, 0.45)
    blot = F.interpolate(torch.rand(b, 1, 6, 6, device=dev), (h, w),
                         mode='bicubic', align_corners=False)
    bg = (lv * (0.4 + blot) + torch.randn(b, 1, h, w, device=dev)
          * 0.015).clamp(0.0, 1.0)
    return torch.where(valid > 0.5, x, bg)


# the IR cameras' look: a random curve (gamma, black and white
# points, an S), light falling off away from the face, the IR light
# up close pushing skin to white, a little blur, sensor noise
def ir_look(x, cx, cy):
    b = x.shape[0]
    dev = x.device

    def r(lo, hi):
        return torch.empty(b, 1, 1, 1, device=dev).uniform_(lo, hi)

    x = x.clamp(1e-4, 1.0) ** r(0.45, 1.4)
    lo, hi = r(0.0, 0.1), r(0.6, 1.0)
    x = ((x - lo) / (hi - lo)).clamp(0.0, 1.0)
    s = r(0.0, 1.0)
    x = x * (1 - s) + s * x * x * (3 - 2 * x)
    _, _, h, w = x.shape
    gy = torch.linspace(0, 1, h, device=dev).view(1, 1, h, 1)
    gx = torch.linspace(0, 1, w, device=dev).view(1, 1, 1, w)
    d2 = (gx - cx.view(b, 1, 1, 1)) ** 2 + (gy - cy.view(b, 1, 1, 1)) ** 2
    x = x * (1.0 - r(0.0, 1.2) * d2).clamp(0.1, 1.0)
    # the lamp lights the face: its light set to 40% to 85% white
    near = torch.exp(-d2 / 0.02)
    lit = (x * near).sum((2, 3), keepdim=True) / \
        near.sum((2, 3), keepdim=True).clamp(min=1e-6)
    x = x * (r(0.4, 0.85) / lit.clamp(min=0.02)).clamp(0.5, 10.0)
    if torch.rand(1).item() < 0.5:
        x = F.avg_pool2d(x, 3, 1, 1)
    x = x + torch.randn_like(x) * r(0.0, 0.02)
    return x.clamp(0.0, 1.0)


# find: in the frame, the middle between the eyes and the face's
# size
# the eye stage: an eye's outer corner and pupil
class EyeNet(StageConv):
    def __init__(self):
        super().__init__(EYE, 4)


class FindBlock(nn.Module):
    def __init__(self, cin, cout, stride=1):
        super().__init__()
        self.body = nn.Sequential(
            nn.Conv2d(cin, cout, 3, stride, 1, bias=False),
            nn.GroupNorm(8, cout), nn.SiLU(),
            nn.Conv2d(cout, cout, 3, 1, 1, bias=False),
            nn.GroupNorm(8, cout))
        self.skip = (nn.Identity() if cin == cout and stride == 1
                     else nn.Conv2d(cin, cout, 1, stride, bias=False))

    def forward(self, x):
        return F.silu(self.body(x) + self.skip(x))


class FindNet(nn.Module):
    def __init__(self):
        super().__init__()
        self.stem = nn.Sequential(
            nn.Conv2d(1, 32, 5, 1, 2, bias=False),
            nn.GroupNorm(8, 32), nn.SiLU())
        self.full = FindBlock(32, 32)
        self.at_half = nn.Sequential(FindBlock(32, 48, 2),
                                     FindBlock(48, 48))
        self.at_quarter = nn.Sequential(FindBlock(48, 96, 2),
                                        FindBlock(96, 96))
        self.at_eighth = nn.Sequential(FindBlock(96, 160, 2),
                                       FindBlock(160, 160))
        self.to_quarter = nn.Conv2d(160, 96, 1)
        self.refine_quarter = FindBlock(96, 96)
        self.to_half = nn.Conv2d(96, 48, 1)
        self.refine_half = FindBlock(48, 48)
        self.heat = nn.Conv2d(48, 1, 1)
        self.log_temp = nn.Parameter(torch.tensor(np.log(10.0)))
        axis = (torch.arange(FIND // 2) + 0.5) / (FIND // 2)
        self.register_buffer('axis', axis)
        self.gap = nn.Sequential(nn.Linear(160 + 48, 96), nn.SiLU(),
                                 nn.Linear(96, 1), nn.Sigmoid())

    def locate(self, x):
        full = self.full(self.stem(normalize(x)))
        half = self.at_half(full)
        quarter = self.at_quarter(half)
        eighth = self.at_eighth(quarter)
        up = F.interpolate(self.to_quarter(eighth),
                           size=quarter.shape[-2:], mode='bilinear',
                           align_corners=False)
        up = self.refine_quarter(up + quarter)
        up = F.interpolate(self.to_half(up), size=half.shape[-2:],
                           mode='bilinear', align_corners=False)
        feature = self.refine_half(up + half)
        heat = self.heat(feature)
        flat = heat.flatten(2) * self.log_temp.exp().clamp(1.0, 100.0)
        prob = torch.softmax(flat, 2).reshape_as(heat)
        px = (prob.sum(2) * self.axis).sum((1, 2))
        py = (prob.sum(3) * self.axis).sum((1, 2))
        local = (feature * prob).sum((2, 3))
        scene = F.adaptive_avg_pool2d(eighth, 1).flatten(1)
        gap = self.gap(torch.cat([scene, local], 1))[:, 0]
        return torch.stack([px, py, gap], 1), heat

    def forward(self, x):
        return self.locate(x)[0]


def find_loss(model, x, y):
    out, heat = model.locate(x)
    gx = model.axis.view(1, 1, 1, -1)
    gy = model.axis.view(1, 1, -1, 1)
    sigma = 1.5 / heat.shape[-1]
    mx = y[:, 0].view(-1, 1, 1, 1)
    my = y[:, 1].view(-1, 1, 1, 1)
    d2 = (gx - mx) ** 2 + (gy - my) ** 2
    target = torch.softmax((-d2 / (2 * sigma * sigma)).flatten(1), 1)
    log_prob = torch.log_softmax(heat.flatten(1), 1)
    heat_loss = -(target * log_prob).sum(1).mean()
    point_loss = F.smooth_l1_loss(out[:, :2], y[:, :2], beta=0.01)
    gap_loss = F.smooth_l1_loss(
        out[:, 2].clamp(min=1e-4).log(),
        y[:, 2].clamp(min=1e-4).log(), beta=0.05)
    # the map itself weighs half: one sharp peak on the truth, none
    # on a lookalike that would nudge the weighted point
    return point_loss + gap_loss * 0.1 + heat_loss * HEAT_WEIGHT


# a smooth random weight map of a few blobs; its slope moves the
# picture, so areas grow where it rises and shrink where it falls.
# the displacement in the crop's -1..1 units, at most `most`
def bend_field(b, out, most, dev):
    g = torch.linspace(-1, 1, out, device=dev)
    gy, gx = torch.meshgrid(g, g, indexing='ij')
    phi = torch.zeros(b, out, out, device=dev)
    many = torch.randint(3, 9, (b, 1, 1), device=dev)
    for k in range(8):
        mx = torch.empty(b, 1, 1, device=dev).uniform_(-1, 1)
        my = torch.empty(b, 1, 1, device=dev).uniform_(-1, 1)
        sg = torch.empty(b, 1, 1, device=dev).uniform_(0.15, 0.5)
        am = torch.empty(b, 1, 1, device=dev).uniform_(-1, 1)
        am = am * (k < many).float()
        phi = phi + am * torch.exp(-((gx - mx) ** 2 + (gy - my) ** 2)
                                   / (2 * sg * sg))
    dy, dx = torch.gradient(phi, dim=(1, 2))
    d = torch.stack([dx, dy], 1)
    peak = d.flatten(1).abs().amax(1).clamp(min=1e-6).view(b, 1, 1, 1)
    m = torch.empty(b, 1, 1, 1, device=dev).uniform_(0.3, 1.0) * most
    d = d / peak * m
    # never folded: the bend's steepest stretch or squeeze kept under
    # 0.7 (a pixel moving past its neighbour folds the picture)
    step = 2.0 / (out - 1)
    gxx = torch.gradient(d[:, 0], dim=2)[0] / step
    gyy = torch.gradient(d[:, 1], dim=1)[0] / step
    gxy = torch.gradient(d[:, 0], dim=1)[0] / step
    gyx = torch.gradient(d[:, 1], dim=2)[0] / step
    steep = torch.stack([gxx, gyy, gxy, gyx], 1).abs().flatten(1)
    k = (0.7 / steep.amax(1).clamp(min=1e-6)).clamp(max=1.0)
    return d * k.view(b, 1, 1, 1)


# where a point of the plain crop lands in the bent one: o with
# o + d(o) = the point, found by stepping
def bend_point(d, x, y):
    o = torch.stack([x * 2 - 1, y * 2 - 1], 1)
    target = o.clone()
    for _ in range(8):
        at = F.grid_sample(d, o.view(-1, 1, 1, 2), align_corners=True)
        o = target - at.view(-1, 2)
    return (o[:, 0] + 1) / 2, (o[:, 1] + 1) / 2


def window(x, p, cx, cy, side, roll, out, bend=0.0):
    b = x.shape[0]
    dev = x.device
    c, s = torch.cos(roll), torch.sin(roll)
    theta = torch.zeros(b, 2, 3, device=dev)
    theta[:, 0, 0] = side * c
    theta[:, 0, 1] = -side * s
    theta[:, 1, 0] = side * s
    theta[:, 1, 1] = side * c
    theta[:, 0, 2] = cx * 2 - 1
    theta[:, 1, 2] = cy * 2 - 1
    grid = F.affine_grid(theta, (b, 1, out, out), align_corners=False)
    d = None
    if bend > 0.0:
        d = bend_field(b, out, bend, dev)
        moved = F.affine_grid(torch.eye(2, 3, device=dev)[None].expand(
            b, 2, 3), (b, 1, out, out), align_corners=False)
        moved = moved + d.permute(0, 2, 3, 1)
        ones = torch.ones(b, out, out, 1, device=dev)
        grid = torch.einsum('bij,bhwj->bhwi', theta,
                            torch.cat([moved, ones], 3))
    win = F.grid_sample(x, grid, align_corners=False)
    valid = F.grid_sample(torch.ones_like(x), grid, align_corners=False)
    win = backdrop(win, valid)
    q = p.clone()
    for k in range(5):
        dx = p[:, 2 * k] - cx
        dy = p[:, 2 * k + 1] - cy
        q[:, 2 * k] = (c * dx + s * dy) / side + 0.5
        q[:, 2 * k + 1] = (-s * dx + c * dy) / side + 0.5
        if d is not None:
            q[:, 2 * k], q[:, 2 * k + 1] = bend_point(
                d, q[:, 2 * k], q[:, 2 * k + 1])
    q[:, 10] = p[:, 10] / side
    return win, q


# things over the face: up to three, each half the time; round
# blobs (cups, hair), long thin strips (pens, glasses, hair), or a
# real hand and forearm (HanCo); blobs and strips their own
# surface, never from a face: flat, a gradient, blotches or
# stripes. the labels stay where the eyes are
def blob(gx, gy, mx, my, rx, ry, an):
    ux = (gx - mx) * torch.cos(an) + (gy - my) * torch.sin(an)
    uy = -(gx - mx) * torch.sin(an) + (gy - my) * torch.cos(an)
    return torch.sigmoid((1 - (ux / rx) ** 2 - (uy / ry) ** 2) * 8)


# a cover's surface: flat, a gradient, blotches or stripes, with
# grain, at a random level
def surface(b, h, gx, gy, dev):
    def u(lo, hi):
        return torch.empty(b, 1, 1, device=dev).uniform_(lo, hi)

    base = u(0.05, 0.95)
    an = u(0, np.pi)
    along = gx * torch.cos(an) + gy * torch.sin(an)
    grad = base + u(-0.4, 0.4) * (along - 0.5)
    blot = base * (0.5 + F.interpolate(
        torch.rand(b, 1, 8, 8, device=dev), (h, h), mode='bicubic',
        align_corners=False)[:, 0])
    stripe = base * (0.7 + 0.3 * torch.sin(along * u(20, 80)))
    kind = torch.randint(4, (b, 1, 1), device=dev)
    fill = torch.where(kind == 0, base.expand(b, h, h),
           torch.where(kind == 1, grad,
           torch.where(kind == 2, blot, stripe)))
    fill = fill + torch.randn(b, h, h, device=dev) * 0.01
    return fill.clamp(0, 1).unsqueeze(1)


# the real hands (grey cutouts, masks), set once loaded
BANK = None


# one real hand over each crop that uses it: a random cutout,
# its frame size times the face (0.6 to 1.6), turned any way,
# its middle at (mx, my); its picture and its mask in the crop
def real_hand(b, h, mx, my, sz, an, dev):
    hands, masks = BANK
    k = torch.randint(len(hands), (b,), device=dev)
    scale = sz.view(b) * torch.empty(b, device=dev).uniform_(0.6, 1.6)
    c, s = torch.cos(an.view(b)), torch.sin(an.view(b))
    theta = torch.zeros(b, 2, 3, device=dev)
    theta[:, 0, 0], theta[:, 0, 1] = c / scale, s / scale
    theta[:, 1, 0], theta[:, 1, 1] = -s / scale, c / scale
    ox = 1 - 2 * mx.view(b)
    oy = 1 - 2 * my.view(b)
    theta[:, 0, 2] = (c * ox + s * oy) / scale
    theta[:, 1, 2] = (-s * ox + c * oy) / scale
    grid = F.affine_grid(theta, (b, 1, h, h), align_corners=False)
    pic = F.grid_sample(hands[k], grid, align_corners=False)
    m = F.grid_sample(masks[k], grid, align_corners=False)
    tone = torch.empty(b, 1, 1, 1, device=dev).uniform_(0.7, 1.5)
    return (pic * tone).clamp(0, 1), m


# the user's own frames, set once loaded
OWN = None


# a random section of one of his frames (a fifth to two thirds of
# it), turned, laid round (mx, my) at 0.4 to 1.2 times the face, cut
# by a blob with a ragged edge
def own_patch(b, h, mx, my, sz, an, gx, gy, dev):
    k = torch.randint(len(OWN), (b,), device=dev)
    cx = torch.empty(b, device=dev).uniform_(-0.6, 0.6)
    cy = torch.empty(b, device=dev).uniform_(-0.6, 0.6)
    sec = torch.empty(b, device=dev).uniform_(0.2, 0.67)
    area = sz.view(b) * torch.empty(b, device=dev).uniform_(0.4, 1.2)
    k2 = sec / area
    c, s = torch.cos(an.view(b)), torch.sin(an.view(b))
    theta = torch.zeros(b, 2, 3, device=dev)
    theta[:, 0, 0], theta[:, 0, 1] = c * k2, s * k2
    theta[:, 1, 0], theta[:, 1, 1] = -s * k2, c * k2
    ox = -(2 * mx.view(b) - 1)
    oy = -(2 * my.view(b) - 1)
    theta[:, 0, 2] = cx + (c * ox + s * oy) * k2
    theta[:, 1, 2] = cy + (-s * ox + c * oy) * k2
    grid = F.affine_grid(theta, (b, 1, h, h), align_corners=False)
    pic = F.grid_sample(OWN[k], grid, align_corners=False)
    a = area.view(b, 1, 1) / 2
    m = blob(gx, gy, mx, my, a, a * torch.empty(
        b, 1, 1, device=dev).uniform_(0.5, 1.0), an)
    rag = F.interpolate(torch.rand(b, 1, 6, 6, device=dev), (h, h),
                        mode='bicubic', align_corners=False)[:, 0]
    m = ((m + (rag - 0.5) * 0.8 - 0.5) * 6 + 0.5).clamp(0, 1)
    return pic, m.unsqueeze(1)


def cover(win, q):
    b, _, h, w = win.shape
    dev = win.device
    g = torch.linspace(0, 1, h, device=dev)
    gy, gx = torch.meshgrid(g, g, indexing='ij')

    def u(lo, hi):
        return torch.empty(b, 1, 1, device=dev).uniform_(lo, hi)

    out = win
    sz = q[:, 10].view(b, 1, 1)
    for _ in range(3):
        use = (torch.rand(b, device=dev) < 0.5).float().view(b, 1, 1)
        mx = q[:, 8].view(b, 1, 1) + sz * u(-0.4, 0.4)
        my = q[:, 9].view(b, 1, 1) + sz * u(-0.35, 0.45)
        an = u(0, np.pi)
        # his own frames 50%, hands 30%, blobs 15%, strips 5%
        pick = torch.rand(b, 1, 1, device=dev)
        kind = torch.where(pick < 0.5, 3, torch.where(pick < 0.8, 2,
                           torch.where(pick < 0.95, 0, 1)))
        round_ = blob(gx, gy, mx, my, sz * u(0.1, 0.35),
                      sz * u(0.08, 0.3), an)
        strip = blob(gx, gy, mx, my, sz * u(0.3, 0.9),
                     sz * u(0.015, 0.06), an)
        m = torch.where(kind == 0, round_, strip).unsqueeze(1)
        fill = surface(b, h, gx, gy, dev)
        if BANK is not None:
            pic, hm = real_hand(b, h, mx, my, sz, an, dev)
            hand = (kind == 2).view(b, 1, 1, 1)
            m = torch.where(hand, hm, m)
            fill = torch.where(hand, pic, fill)
        own = (kind == 3).view(b, 1, 1, 1)
        if OWN is not None:
            pic, om = own_patch(b, h, mx, my, sz, an, gx, gy, dev)
            # his frames never over an eye: clear round each center
            for e in (4, 6):
                d2 = ((gx - q[:, e].view(b, 1, 1)) ** 2
                      + (gy - q[:, e + 1].view(b, 1, 1)) ** 2)
                r = sz * 0.12
                om = om * ((d2.sqrt() - r) / r).clamp(0, 1).unsqueeze(1)
            m = torch.where(own, om, m)
            fill = torch.where(own, pic, fill)
        else:
            m = m * (~own).float()
        m = m * use.unsqueeze(1)
        # never both eyes: a cover over both eye centers is dropped
        both = torch.ones(b, device=dev, dtype=torch.bool)
        for e in (4, 6):
            ix = (q[:, e] * w).long().clamp(0, w - 1)
            iy = (q[:, e + 1] * h).long().clamp(0, h - 1)
            both &= m[torch.arange(b, device=dev), 0, iy, ix] > 0.5
        m = m * (~both).float().view(b, 1, 1, 1)
        out = out * (1 - m) + fill * m
    return mouth_hand(out, q, gx, gy)


# half the crops: a hand over the mouth (a HanCo hand or one of his
# frames), below the eye line by about the eyes' spacing, the eyes
# left clear
def mouth_hand(out, q, gx, gy):
    b, _, h, _ = out.shape
    dev = out.device
    if BANK is None:
        return out

    def u(lo, hi):
        return torch.empty(b, 1, 1, device=dev).uniform_(lo, hi)

    sz = q[:, 10].view(b, 1, 1)
    ex = ((q[:, 4] + q[:, 6]) / 2).view(b, 1, 1)
    ey = ((q[:, 5] + q[:, 7]) / 2).view(b, 1, 1)
    dx = (q[:, 6] - q[:, 4]).view(b, 1, 1)
    dy = (q[:, 7] - q[:, 5]).view(b, 1, 1)
    # down the face: the eye line turned a quarter
    nx, ny = -dy, dx
    down = u(1.0, 1.6)
    mx = ex + nx * down + dx * u(-0.3, 0.3)
    my = ey + ny * down + dy * u(-0.3, 0.3)
    an = u(0, np.pi)
    pic, m = real_hand(b, h, mx, my, sz, an, dev)
    if OWN is not None:
        opic, om = own_patch(b, h, mx, my, sz * 1.3, an, gx, gy, dev)
        # only the lit parts: his hand, not the dark room
        om = om * ((opic - 0.2) / 0.15).clamp(0, 1)
        mine = (torch.rand(b, 1, 1, 1, device=dev) < 0.5)
        pic = torch.where(mine, opic, pic)
        m = torch.where(mine, om, m)
    # a large blob over the whole mouth too, in the hand's tones
    big = blob(gx, gy, mx, my, sz * u(0.3, 0.5), sz * u(0.25, 0.4),
               an).unsqueeze(1)
    fill = surface(b, h, gx, gy, dev)
    pic = torch.where(m > 0.5, pic, fill)
    m = torch.maximum(m, big)
    for e in (4, 6):
        d2 = ((gx - q[:, e].view(b, 1, 1)) ** 2
              + (gy - q[:, e + 1].view(b, 1, 1)) ** 2)
        r = sz * 0.12
        m = m * ((d2.sqrt() - r) / r).clamp(0, 1).unsqueeze(1)
    # near the lamp a hand is bright
    pic = (pic * u(1.0, 1.8).unsqueeze(1)).clamp(0, 1)
    use = (torch.rand(b, 1, 1, 1, device=dev) < 0.5).float()
    m = m * use
    return out * (1 - m) + pic * m


# frames flipped left to right half the time: the points with them,
# image left staying first
def flip(x, p):
    m = torch.rand(x.shape[0], device=x.device) < 0.5
    x = torch.where(m.view(-1, 1, 1, 1), x.flip(3), x)
    q = p.clone()
    q[:, 0:10:2] = torch.where(m.view(-1, 1), 1.0 - p[:, 0:10:2],
                               p[:, 0:10:2])
    s = q.clone()
    s[m, 0:2], s[m, 2:4] = q[m, 2:4], q[m, 0:2]
    s[m, 4:6], s[m, 6:8] = q[m, 6:8], q[m, 4:6]
    return x, s


def rand(b, lo, hi, dev):
    return torch.empty(b, device=dev).uniform_(lo, hi)


# find's answer is target's square: find, then target, gives the
# final points
# a random crop of the picture, always overlapping the face: the
# face a third of it to 2.5 times it (a crop of the face alone, as
# the cameras see it up close), anywhere about it, rolled; the
# face's square in the crop's shares
# the lamp leaves all but the face dark, covers too: a fuzzy,
# ragged oval round it, the rest 3% to 30% as bright, 90% of the time
def surround_dark(x, cx, cy, sz):
    b, _, h, w = x.shape
    dev = x.device

    def r(lo, hi):
        return torch.empty(b, 1, 1, 1, device=dev).uniform_(lo, hi)

    gy = torch.linspace(0, 1, h, device=dev).view(1, 1, h, 1)
    gx = torch.linspace(0, 1, w, device=dev).view(1, 1, 1, w)
    d = ((gx - cx.view(b, 1, 1, 1)) ** 2
         + (gy - cy.view(b, 1, 1, 1)) ** 2).sqrt()
    rad = sz.view(b, 1, 1, 1) * r(0.55, 0.9)
    rag = F.interpolate(torch.rand(b, 1, 5, 5, device=dev), (h, w),
                        mode='bicubic', align_corners=False)
    edge = (d / rad + (rag - 0.5) * 0.6 - 1.0) / r(0.15, 0.5)
    dark = torch.sigmoid(edge * 4)
    on = (torch.rand(b, 1, 1, 1, device=dev) < 0.9).float()
    return x * (1 - on * dark * (1 - r(0.03, 0.3)))


# smooth noise in 0..1: octaves of random grids, bicubic, summed
def perlin(b, h, cells, dev):
    out = torch.zeros(b, 1, h, h, device=dev)
    amp, tot = 1.0, 0.0
    for n in cells:
        g = torch.rand(b, 1, n, n, device=dev)
        out = out + amp * F.interpolate(g, (h, h), mode='bicubic',
                                        align_corners=False)
        tot += amp
        amp *= 0.5
    return (out / tot).clamp(0, 1)


def smooth(t):
    t = t.clamp(0, 1)
    return t * t * (3 - 2 * t)


# most pictures (CANVAS): no photo round the face. A black canvas,
# a noise layer brushed on in perlin patches, then the eyes brushed
# in from the photo: a soft perlin-edged section round them that gets
# most of the face, never a plain oval
def eye_canvas(x, q):
    b, _, h, w = x.shape
    dev = x.device

    def u(lo, hi):
        return torch.empty(b, 1, 1, 1, device=dev).uniform_(lo, hi)

    out = torch.zeros(b, 1, h, w, device=dev)
    for _ in range(3):
        tone = perlin(b, h, (3, 6, 12, 24), dev) * u(0.1, 0.8)
        cut = perlin(b, h, (2, 4, 8), dev)
        m = smooth((cut - u(0.4, 0.75)) / u(0.2, 0.5))
        out = out * (1 - m) + tone * m
    g = torch.linspace(0, 1, h, device=dev)
    gy, gx = torch.meshgrid(g, g, indexing='ij')
    gx, gy = gx[None, None], gy[None, None]
    ex = ((q[:, 0] + q[:, 2]) / 2).view(b, 1, 1, 1)
    ey = ((q[:, 1] + q[:, 3]) / 2).view(b, 1, 1, 1)
    dx = (q[:, 2] - q[:, 0]).view(b, 1, 1, 1)
    dy = (q[:, 3] - q[:, 1]).view(b, 1, 1, 1)
    sc = (dx * dx + dy * dy).sqrt().clamp(min=1e-4)
    along = ((gx - ex) * dx + (gy - ey) * dy) / sc
    # its middle down the face a little, toward the nose
    across = (-(gx - ex) * dy + (gy - ey) * dx) / sc - sc * u(0.2, 0.5)
    d = ((along / (sc * u(1.1, 1.6))) ** 2
         + (across / (sc * u(1.0, 1.5))) ** 2).sqrt()
    edge = (1 - d) + (perlin(b, h, (3, 6, 12), dev) - 0.5) * 0.9
    m = smooth(edge / u(0.4, 0.9))
    # the rest keep the whole photo: real heads, hair, shoulders
    use = torch.rand(b, 1, 1, 1, device=dev) < CANVAS
    return torch.where(use, out * (1 - m) + x * m, x)


# a third of the faces get lids painted over their open eyes: skin
# from above the eye drawn down over it from the brow side, part or
# all the way shut, with a soft dark lash line where it stops; both
# eyes mostly, one alone a fifth of the time
def close_eyes(x, q):
    b, _, h, w = x.shape
    dev = x.device

    def u(lo, hi):
        return torch.empty(b, 1, 1, device=dev).uniform_(lo, hi)

    g = torch.linspace(0, 1, h, device=dev)
    gy, gx = torch.meshgrid(g, g, indexing='ij')
    use = (torch.rand(b, 1, 1, device=dev) < 1 / 3).float()
    shut = u(0.4, 1.0)
    lash = u(0.3, 0.7)
    one = torch.rand(b, 1, 1, device=dev) < 0.2
    pick = torch.rand(b, 1, 1, device=dev) < 0.5
    out = x
    # each eye: its outer corner (0, 2) and pupil (4, 6)
    for k, (oc, pu) in enumerate(((0, 4), (2, 6))):
        px, py = q[:, pu].view(b, 1, 1), q[:, pu + 1].view(b, 1, 1)
        dx = px - q[:, oc].view(b, 1, 1)
        dy = py - q[:, oc + 1].view(b, 1, 1)
        d = (dx * dx + dy * dy).sqrt().clamp(min=1e-4)
        half = d * 1.25
        ax, ay = dx / d, dy / d
        along = (gx - px) * ax + (gy - py) * ay
        # down the face: the eye line turned a quarter, by which
        # side of the face this eye is on
        sgn = 1.0 if k == 0 else -1.0
        across = sgn * (-(gx - px) * ay + (gy - py) * ax)
        tall = half * 0.55
        inside = ((along / half) ** 2 + (across / tall) ** 2).sqrt()
        eye = smooth((1 - inside) / 0.45)
        edge = -tall + 2 * tall * shut
        lid = eye * ((edge - across) / (tall * 0.15)).clamp(0, 1)
        line = eye * torch.exp(-((across - edge) / (tall * 0.12)) ** 2)
        # the lid's tone: the skin between the eye and the brow
        sx = px + sgn * ay * half * 0.9
        sy = py - sgn * ax * half * 0.9
        spot = torch.stack([sx * 2 - 1, sy * 2 - 1], -1).view(b, 1, 1, 2)
        tone = F.grid_sample(F.avg_pool2d(x, 5, 1, 2), spot,
                             align_corners=False).view(b, 1, 1)
        on = use * torch.where(one, (pick == (k == 0)).float(), 1.0)
        m = (lid * on).unsqueeze(1)
        tone = tone * u(0.85, 1.0)
        out = out * (1 - m) + tone.unsqueeze(1) * m
        out = out * (1 - (line * on * lash).unsqueeze(1))
    return out


# the hair, set once loaded: grey, masks, eyes (x, y, sc)
HAIRSET = None
# his own hair frames (myhair/cam*), set once loaded
MYHAIR = None


# every face gets another face's hair, laid by the eyes: that face's
# eye middle on this one's, its sc to this one's (90% to 115%),
# turned a little, mirrored half the time, at a random tone; the
# eyes themselves always left clear
def paint_hair(x, q):
    if HAIRSET is None:
        return x
    hair, masks, eyes = HAIRSET
    b, _, h, w = x.shape
    dev = x.device
    k = torch.randint(len(hair), (b,), device=dev)
    se = eyes[k]
    ex = (q[:, 0] + q[:, 2]) / 2
    ey = (q[:, 1] + q[:, 3]) / 2
    sc = torch.hypot(q[:, 2] - q[:, 0], q[:, 3] - q[:, 1])
    an = torch.atan2(q[:, 3] - q[:, 1], q[:, 2] - q[:, 0])
    an = an + torch.empty(b, device=dev).uniform_(-0.15, 0.15)
    k2 = se[:, 2] / sc.clamp(min=1e-4) * torch.empty(
        b, device=dev).uniform_(0.9, 1.15)
    flip = torch.where(torch.rand(b, device=dev) < 0.5, -1.0, 1.0)
    c, s = torch.cos(an) * k2, torch.sin(an) * k2
    # crop point g to the hair picture: src = A (g - e) + e_src
    a00, a01, a10, a11 = c * flip, s, -s * flip, c
    tx = se[:, 0] - (a00 * ex + a01 * ey)
    ty = se[:, 1] - (a10 * ex + a11 * ey)
    theta = torch.zeros(b, 2, 3, device=dev)
    theta[:, 0, 0], theta[:, 0, 1] = a00, a01
    theta[:, 1, 0], theta[:, 1, 1] = a10, a11
    theta[:, 0, 2] = a00 + a01 + 2 * tx - 1
    theta[:, 1, 2] = a10 + a11 + 2 * ty - 1
    grid = F.affine_grid(theta, (b, 1, h, w), align_corners=False)
    pic = F.grid_sample(hair[k], grid, align_corners=False)
    # half the time his own hair fills the shape: a random section of
    # one of his hair frames, as his cameras see it
    if MYHAIR is not None:
        # four tries a face: the first not mostly the dark room, else
        # the brightest
        mine, best = None, None
        for _ in range(4):
            j = torch.randint(len(MYHAIR), (b,), device=dev)
            sec = torch.empty(b, device=dev).uniform_(0.25, 0.6)
            th = torch.zeros(b, 2, 3, device=dev)
            an = torch.empty(b, device=dev).uniform_(-0.4, 0.4)
            th[:, 0, 0] = sec * torch.cos(an)
            th[:, 0, 1] = -sec * torch.sin(an)
            th[:, 1, 0] = sec * torch.sin(an)
            th[:, 1, 1] = sec * torch.cos(an)
            th[:, :, 2] = torch.empty(b, 2, device=dev).uniform_(-0.5, 0.5)
            g2 = F.affine_grid(th, (b, 1, h, w), align_corners=False)
            try_ = F.grid_sample(MYHAIR[j], g2, align_corners=False)
            lit = try_.mean((1, 2, 3))
            if mine is None:
                mine, best = try_, lit
                continue
            take = (best < 0.15) & (lit > best)
            mine = torch.where(take.view(b, 1, 1, 1), try_, mine)
            best = torch.where(take, lit, best)
        his = torch.rand(b, 1, 1, 1, device=dev) < 0.5
        pic = torch.where(his, mine, pic)
    m = F.grid_sample(masks[k], grid, align_corners=False)
    tone = torch.empty(b, 1, 1, 1, device=dev).uniform_(0.3, 1.3)
    g = torch.linspace(0, 1, h, device=dev)
    gy, gx = torch.meshgrid(g, g, indexing='ij')
    for e in (4, 6):
        d = ((gx - q[:, e].view(b, 1, 1)) ** 2
             + (gy - q[:, e + 1].view(b, 1, 1)) ** 2).sqrt()
        r = sc.view(b, 1, 1) * 0.3
        m = m * ((d - r) / r).clamp(0, 1).unsqueeze(1)
    return x * (1 - m) + (pic * tone).clamp(0, 1) * m


# his own frames (a twelfth number of 1): real pictures of his
# cameras already, so only cropped, rolled and bent, never painted
def his_rows(p):
    if p.shape[1] < 12:
        return torch.zeros(p.shape[0], dtype=torch.bool, device=p.device)
    return p[:, 11] > 0.5


# every crop's painting: hair, then (training) closed lids, the IR
# look, the canvas, covers and the dark round it; his own frames
# kept as they are
# the lamp close: most faces (BLOW) lit too hot, a third each way:
# a soft hot spot 2 to 6 times; burnt as camera 2 burns them (the
# face over its mean brightness to the power 2 to 3, cut at white:
# skin white, eyes dark grey); or washed out, the eyes gone too
def blow_out(x, cx, cy, sz):
    b, _, h, w = x.shape
    dev = x.device

    def r(lo, hi):
        return torch.empty(b, 1, 1, 1, device=dev).uniform_(lo, hi)

    gy = torch.linspace(0, 1, h, device=dev).view(1, 1, h, 1)
    gx = torch.linspace(0, 1, w, device=dev).view(1, 1, 1, w)
    mx = cx.view(b, 1, 1, 1) + sz.view(b, 1, 1, 1) * r(-0.3, 0.3)
    my = cy.view(b, 1, 1, 1) + sz.view(b, 1, 1, 1) * r(-0.2, 0.4)
    rad = sz.view(b, 1, 1, 1) * r(0.4, 1.0)
    spot = torch.exp(-((gx - mx) ** 2 + (gy - my) ** 2) / (rad * rad))
    gain = 1 + (r(2.0, 6.0) - 1) * spot
    hot = (x * gain).clamp(0, 1)
    by = cy.view(b, 1, 1, 1) + sz.view(b, 1, 1, 1) * r(0.0, 0.15)
    brad = sz.view(b, 1, 1, 1) * r(0.3, 0.45)
    d = ((gx - mx) ** 2 + (gy - by) ** 2) / (brad * brad)
    wt = torch.exp(-d * d)
    mid = ((x * wt).sum((2, 3), keepdim=True)
           / wt.sum((2, 3), keepdim=True)).clamp(min=0.05)
    white = ((x / mid) ** r(2.0, 3.0) * r(1.0, 1.4)).clamp(0, 1)
    burn = x * (1 - wt) + white * wt
    # washed out completely: the eyes gone with the skin
    flat = torch.exp(-d) ** 0.3
    gone = (x * (1 + (r(3.0, 10.0) - 1) * flat)).clamp(0, 1)
    kind = torch.randint(3, (b, 1, 1, 1), device=dev)
    lit = torch.where(kind == 0, hot,
                      torch.where(kind == 1, burn, gone))
    on = torch.rand(b, 1, 1, 1, device=dev) < BLOW
    return torch.where(on, lit, x)


def paint(win, q, p, aug):
    plain = win
    win = paint_hair(win, q)
    if aug:
        b, dev = win.shape[0], win.device
        # half normal: no hair painted, the face on its canvas, its
        # own contrast stretched: its darkest 2% black, brightest 2%
        # white
        calm = eye_canvas(plain, q)
        _, _, h, w = calm.shape
        gy = torch.linspace(0, 1, h, device=dev).view(1, 1, h, 1)
        gx = torch.linspace(0, 1, w, device=dev).view(1, 1, 1, w)
        near = ((gx - q[:, 8].view(-1, 1, 1, 1)) ** 2
                + (gy - q[:, 9].view(-1, 1, 1, 1)) ** 2) \
            < (q[:, 10].view(-1, 1, 1, 1) * 0.35) ** 2
        face = torch.where(near, calm, torch.nan).flatten(1)
        lo = torch.nanquantile(face, 0.02, dim=1).view(-1, 1, 1, 1)
        hi = torch.nanquantile(face, 0.98, dim=1).view(-1, 1, 1, 1)
        lo, hi = lo.nan_to_num(0.0), hi.nan_to_num(1.0)
        calm = (calm - lo) / (hi - lo).clamp(min=0.05)
        calm = (calm + torch.randn_like(calm) * 0.01).clamp(0, 1)
        # half rough: the camera's look, burns, covers
        win = close_eyes(win, q)
        win = ir_look(win, q[:, 8], q[:, 9])
        win = eye_canvas(win, q)
        win = cover(win, q)
        win = blow_out(win, q[:, 8], q[:, 9], q[:, 10])
        win = surround_dark(win, q[:, 8], q[:, 9], q[:, 10])
        rough = (torch.rand(b, device=dev) < ROUGH).view(-1, 1, 1, 1)
        win = torch.where(rough, win, calm)
        return torch.where(his_rows(p).view(-1, 1, 1, 1), plain, win)
    win = eye_canvas(win, q)
    return surround_dark(win, q[:, 8], q[:, 9], q[:, 10])


def find_batch(frames, pts, aug):
    x = frames.float()[:, None] / 255.0
    p = pts
    b, dev = x.shape[0], x.device
    if aug:
        x, p = flip(x, p)
    side = p[:, 10]
    # its size by a log-uniform share: as many big faces as small
    zoom = torch.exp(rand(b, np.log(0.33), np.log(2.5), dev))
    ws = side / (zoom if aug else 1 / 1.8)
    # both eyes always in it: anywhere they stay within its middle
    # 85%, it never narrower than the eyes need
    ex0 = torch.minimum(p[:, 0], p[:, 2])
    ex1 = torch.maximum(p[:, 0], p[:, 2])
    ey0 = torch.minimum(p[:, 1], p[:, 3])
    ey1 = torch.maximum(p[:, 1], p[:, 3])
    ws = torch.maximum(ws, torch.maximum(ex1 - ex0, ey1 - ey0) / 0.8)
    roll = rand(b, -TURN, TURN, dev) if aug else torch.zeros(b, device=dev)
    # never past the picture, turned corners too: at most its size
    reach = torch.cos(roll).abs() + torch.sin(roll).abs()
    ws = torch.minimum(ws, 1.0 / reach)
    e = ws * reach / 2
    h = ws * 0.425
    lo_x = torch.maximum(ex1 - h, e)
    hi_x = torch.maximum(torch.minimum(ex0 + h, 1 - e), lo_x)
    lo_y = torch.maximum(ey1 - h, e)
    hi_y = torch.maximum(torch.minimum(ey0 + h, 1 - e), lo_y)
    t = rand(b, 0, 1, dev) if aug else torch.full((b,), 0.5, device=dev)
    u = rand(b, 0, 1, dev) if aug else torch.full((b,), 0.5, device=dev)
    wx = (lo_x + (hi_x - lo_x) * t).clamp(e, 1 - e)
    wy = (lo_y + (hi_y - lo_y) * u).clamp(e, 1 - e)
    win, q = window(x, p, wx, wy, ws, roll, FIND, BEND if aug else 0.0)
    # the face lit first, the covers laid over it after
    win = paint(win, q, p, aug)
    # measured on the face: the outer corners' midpoint (the nose
    # bridge) and their distance
    dx, dy = q[:, 2] - q[:, 0], q[:, 3] - q[:, 1]
    y = torch.stack([(q[:, 0] + q[:, 2]) / 2, (q[:, 1] + q[:, 3]) / 2,
                     torch.sqrt(dx * dx + dy * dy)], 1)
    return win, y


# the face's square cut out (jittered as find misses, rolled), at
# SIDE: the four points, the scale (the outer corners' distance),
# the visible bit
def target_batch(frames, pts, aug):
    x = frames.float()[:, None] / 255.0
    p = pts
    b, dev = x.shape[0], x.device
    if aug:
        x, p = flip(x, p)
    # on the outer corners' midpoint, SPAN times their distance, as
    # find's answer gives it; jittered as find misses
    sc = torch.hypot(p[:, 2] - p[:, 0], p[:, 3] - p[:, 1])
    side = sc * SPAN * (rand(b, 0.9, 1.1, dev) if aug else 1.0)
    ex = (p[:, 0] + p[:, 2]) / 2
    ey = (p[:, 1] + p[:, 3]) / 2
    cx = ex + (side * rand(b, -0.12, 0.12, dev) if aug else 0.0)
    cy = ey + (side * rand(b, -0.15, 0.15, dev) if aug else 0.0)
    tilt = TURN * 8 / 15
    roll = rand(b, -tilt, tilt, dev) if aug else torch.zeros(b, device=dev)
    crop, q = window(x, p, cx, cy, side, roll, SIDE,
                     BEND if aug else 0.0)
    crop = paint(crop, q, p, aug)
    y = torch.empty(b, 10, device=dev)
    y[:, :8] = q[:, :8]
    dx, dy = q[:, 0] - q[:, 2], q[:, 1] - q[:, 3]
    y[:, 8] = torch.sqrt(dx * dx + dy * dy)
    y[:, 9] = 1.0
    return crop, y


# the eye stage: one eye, its crop centred between its outer corner
# and pupil, EYE_SPAN of sc across, turned so the eye line is level;
# the right eye mirrored, so the outer corner is always on the
# left. Its two points in the crop's shares. Training shifts, sizes
# and turns the crop as much as target misses; one eye a picture
def eye_batch(frames, pts, aug):
    x = frames.float()[:, None] / 255.0
    p = pts
    b, dev = x.shape[0], x.device
    if aug:
        x, p = flip(x, p)
    # the face first, at FACE, painted as target's is
    ex = (p[:, 4] + p[:, 6]) / 2
    ey = (p[:, 5] + p[:, 7]) / 2
    tilt = TURN * 8 / 15
    roll = (rand(b, -tilt, tilt, dev) if aug
            else torch.zeros(b, device=dev))
    face, q = window(x, p, ex, ey, p[:, 10], roll, FACE,
                     BEND if aug else 0.0)
    face = paint(face, q, p, aug)
    # which eye: at random, else alternating
    right = (torch.rand(b, device=dev) < 0.5 if aug
             else torch.arange(b, device=dev) % 2 == 1)
    oc = torch.where(right.view(-1, 1), q[:, 2:4], q[:, 0:2])
    pu = torch.where(right.view(-1, 1), q[:, 6:8], q[:, 4:6])
    sc = (q[:, 2:4] - q[:, 0:2]).norm(dim=1).clamp(min=1e-3)
    line = torch.atan2(q[:, 3] - q[:, 1], q[:, 2] - q[:, 0])
    # shifted, sized and turned always: held out too (measure's
    # fixed seed), so the score is how far it pulls a miss in
    side = sc * EYE_SPAN * rand(b, 0.85, 1.15, dev)
    cx = (oc[:, 0] + pu[:, 0]) / 2 + side * rand(b, -0.15, 0.15, dev)
    cy = (oc[:, 1] + pu[:, 1]) / 2 + side * rand(b, -0.15, 0.15, dev)
    line = line + rand(b, -EYE_TURN, EYE_TURN, dev)
    # the eye's two points where window moves points (slots 0, 1)
    e = torch.zeros(b, 11, device=dev)
    e[:, 0:2], e[:, 2:4] = oc, pu
    e[:, 10] = sc
    crop, r = window(face, e, cx, cy, side, line, EYE)
    crop = torch.where(right.view(-1, 1, 1, 1), crop.flip(3), crop)
    r[:, 0] = torch.where(right, 1 - r[:, 0], r[:, 0])
    r[:, 2] = torch.where(right, 1 - r[:, 2], r[:, 2])
    return crop, r[:, :4]


# one pass: every face once, the steep ones half again, the
# frontal and in-between ones three quarters (by count: half the
# copies steep, a quarter frontal), in a new order
# an epoch: size pictures, split among the groups by their shares
# (drawn again with repeats where a group is small), each `views`
# times, shuffled
def one_pass(groups, shares, views, size):
    tot = sum(shares)
    out = torch.cat([g[torch.randint(len(g), (int(size * w / tot),))]
                     for g, w in zip(groups, shares)])
    out = out.repeat(views)
    return out[torch.randperm(len(out))]


# the user's own frames with their true points, marked by hand
# (mytest/points.txt: a file, then the image-left and image-right
# outer corners and pupils in its pixels); every epoch is scored on
# them: find through target, as the app runs them
# also every frame marked in the hyperspace app's annotate view
# (its storage's annotate/points.txt; 'skip' lines left out)
ANNOTATED = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', 'install', 'home',
    'hyperspace', '.local', 'state', 'hyperspace')


# every frame he marked with its four points: (name, grey picture
# 0..1, the points in its pixels); mytest's, then annotate's
def own_marked(cache):
    out = []
    for path, root in ((os.path.join(cache, 'mytest', 'points.txt'),
                        os.path.join(cache, 'mytest')),
                       (os.path.join(ANNOTATED, 'annotate', 'points.txt'),
                        ANNOTATED)):
        if not os.path.exists(path):
            continue
        for line in open(path):
            w = line.split()
            if len(w) < 9 or w[0].startswith('#'):
                continue
            a = np.asarray(Image.open(os.path.join(root, w[0]))
                           .convert('L'), np.float32) / 255.0
            q = np.asarray(w[1:9], np.float32)
            # his own left first (the picture's right): picture order
            if q[0] > q[2]:
                q = q[[2, 3, 0, 1, 6, 7, 4, 5]]
            out.append((w[0], a, q))
    return out


# his frames split: mytest's and every OWN_HOLD-th marked one are the
# test, the rest train
OWN_HOLD = 5


def own_split(cache):
    test, train = [], []
    for i, (nm, a, p) in enumerate(own_marked(cache)):
        keep = (not '/' in nm) or i % OWN_HOLD == 0
        (test if keep else train).append((nm, a, p))
    return test, train


# the test frames as own_score takes them
def own_test(frames):
    return [(nm, torch.tensor(a)[None, None], p) for nm, a, p in frames]


# the training frames as FaceSynthetics' are kept: FRAME grey, and
# the eleven numbers in shares (corners, pupils, the face's middle a
# little under the eyes, its size from sc)
def own_rows(frames):
    if not frames:
        return None, None
    fs, ps = [], []
    for nm, a, p in frames:
        h, w = a.shape
        g = Image.fromarray((a * 255).astype(np.uint8)).resize(
            (FRAME, FRAME), Image.Resampling.BILINEAR)
        fs.append(np.asarray(g, np.uint8))
        q = p / np.array([w, h, w, h, w, h, w, h], np.float32)
        ex, ey = (q[0] + q[2]) / 2, (q[1] + q[3]) / 2
        dx, dy = q[2] - q[0], q[3] - q[1]
        ps.append([*q, ex - dy * 0.45, ey + dx * 0.45, 0.0, 1.0])
    return np.stack(fs), sized(np.asarray(ps, np.float32))


# the app's two steps on one frame: find on the whole frame at FIND,
# target on find's square at SIDE; the four points in its pixels
@torch.no_grad()
def run_pair(find, target, full, dev):
    h = full.shape[-1]
    small = F.interpolate(full, (FIND, FIND), mode='area').to(dev)
    cx, cy, sc = find(small)[0].tolist()
    s = sc * SPAN
    th = torch.tensor([[[s, 0, cx * 2 - 1], [0, s, cy * 2 - 1]]],
                      dtype=torch.float32)
    grid = F.affine_grid(th, (1, 1, SIDE, SIDE), align_corners=False)
    crop = F.grid_sample(full, grid, align_corners=False).to(dev)
    p = target(crop)[0, :8].tolist()
    return np.asarray([(cx + (p[k] - 0.5) * s if k % 2 == 0
                        else cy + (p[k] - 0.5) * s) * h
                       for k in range(8)]), (cx * h, cy * h)


# the eye stage on run_pair's points: each eye cut out by them,
# refined, put back in the frame's pixels
@torch.no_grad()
def run_eyes(eye, full, p, dev):
    h = full.shape[-1]
    q = np.asarray(p, np.float64) / h
    out = q.copy()
    line = np.arctan2(q[3] - q[1], q[2] - q[0])
    sc = max(np.hypot(q[2] - q[0], q[3] - q[1]), 1e-3)
    side = sc * EYE_SPAN
    c, s = np.cos(line), np.sin(line)
    for k in (0, 1):
        oc, pu = q[2 * k:2 * k + 2], q[4 + 2 * k:6 + 2 * k]
        cx, cy = (oc + pu) / 2
        th = torch.tensor([[[side * c, -side * s, cx * 2 - 1],
                            [side * s, side * c, cy * 2 - 1]]],
                          dtype=torch.float32)
        grid = F.affine_grid(th, (1, 1, EYE, EYE), align_corners=False)
        crop = F.grid_sample(full, grid, align_corners=False)
        if k == 1:
            crop = crop.flip(3)
        r = eye(crop.to(dev))[0].tolist()
        for j, slot in ((0, 2 * k), (2, 4 + 2 * k)):
            u = 1 - r[j] if k == 1 else r[j]
            du, dv = (u - 0.5) * side, (r[j + 1] - 0.5) * side
            out[slot] = cx + c * du - s * dv
            out[slot + 1] = cy + s * du + c * dv
    return out * h


# each of his frames: how far find's eye middle lands from the truth,
# and (with a target) each point, as a share of the corner gap
def own_score(own, find, target, dev, eye=None):
    models = [m for m in (find, target, eye) if m is not None]
    was = [m.training for m in models]
    for m in models:
        m.eval()
    shares = []
    for name, full, truth in own:
        h = full.shape[-1]
        gap = np.hypot(truth[2] - truth[0], truth[3] - truth[1])
        if target is None:
            small = F.interpolate(full, (FIND, FIND), mode='area')
            with torch.no_grad():
                cx, cy, sc = find(small.to(dev))[0].tolist()
            d = np.abs([cx * h - (truth[0] + truth[2]) / 2,
                        cy * h - (truth[1] + truth[3]) / 2,
                        sc * h - gap])
        else:
            p, _ = run_pair(find, target, full, dev)
            if eye is not None:
                p = run_eyes(eye, full, p, dev)
            q = np.asarray(p)
            d = np.abs(np.r_[q - truth[:8],
                             np.hypot(q[2] - q[0], q[3] - q[1]) - gap])
        shares.append(d / gap)
    for m, t in zip(models, was):
        m.train(t)
    share = np.mean(shares, 0)
    if target is None:
        return len(own), dict(zip(LABELS[3], share.tolist()))
    if eye is None:
        return len(own), dict(zip(LABELS[10][:9], share.tolist()))
    # the eye stage's rows: both eyes' corners and pupils together
    both = (share[[0, 1, 4, 5]] + share[[2, 3, 6, 7]]) / 2
    return len(own), dict(zip(LABELS[4], both.tolist()))


def fit(model, make, frames, pts, groups, shares, test, names, args,
        name,
        dev, probe=None):
    model.to(dev)
    opt = torch.optim.AdamW(model.parameters(), lr=LR)
    per = (len(one_pass(groups, shares, args.views, args.epoch_size))
           + args.batch - 1) \
        // args.batch
    sched = torch.optim.lr_scheduler.OneCycleLR(
        opt, LR, total_steps=args.epochs * per)
    best, state = None, None
    for epoch in range(args.epochs):
        model.train()
        lost, steps = 0.0, 0
        order = one_pass(groups, shares, args.views, args.epoch_size)
        for k in range(per):
            take = order[k * args.batch:(k + 1) * args.batch]
            x, y = make(frames[take].to(dev, non_blocking=True),
                        pts[take].to(dev), True)
            loss = (find_loss(model, x, y) if isinstance(model, FindNet)
                    else F.smooth_l1_loss(model(x), y, beta=0.01))
            opt.zero_grad()
            loss.backward()
            opt.step()
            sched.step()
            lost += loss.item()
            steps += 1
        err = measure(model, make, frames, pts, test, dev, names)
        many = sum(len(g) for g in test)
        users, mine = probe(model) if probe else (0, {})
        print(f'{name} {epoch + 1}/{args.epochs}   train loss '
              f'{lost / steps:.5f}', flush=True)
        print(f'  eval error, share of the corner gap  '
              f'{"synthetic":>10} {"user":>10}', flush=True)
        print(f'  {"(never trained on)":36} {many:>10} {users:>10}',
              flush=True)
        for k, v in err.items():
            u = f'{mine[k]:.3f}' if k in mine else '-'
            print(f'  {k:36} {v:>10.3f} {u:>10}', flush=True)
        score = float(np.mean(list(err.values())))
        if best is None or score < best:
            best = score
            state = {k: v.detach().cpu().clone()
                     for k, v in model.state_dict().items()}
    model.load_state_dict(state)
    return model.eval(), best


# each output's name: find's three, target's eight points and its
# gap, the eye stage's corner and pupil
LABELS = {3: ['x', 'y', 'gap'],
          10: ['corner L x', 'corner L y', 'corner R x', 'corner R y',
               'pupil L x', 'pupil L y', 'pupil R x', 'pupil R y',
               'gap', 'seen'],
          4: ['corner x', 'corner y', 'pupil x', 'pupil y']}


# each output's mean error on the held-out faces, as a share of the
# outer corners' distance (find's own answer measured against its
# gap; target's crop is SPAN gaps wide, the eye's EYE_SPAN)
@torch.no_grad()
def measure(model, make, frames, pts, test, dev, names):
    model.eval()
    # the same canvases every time: the scores compare epoch to epoch
    cpu = torch.get_rng_state()
    gpu = torch.cuda.get_rng_state(dev) if dev.type == 'cuda' else None
    torch.manual_seed(11)
    misses = []
    for g in test:
        for s in range(0, len(g), 256):
            take = g[s:s + 256]
            x, y = make(frames[take].to(dev), pts[take].to(dev), False)
            out = model(x)
            k = out.shape[1]
            d = (out - y).abs()
            if k == 3:
                d = d / y[:, 2:3]
            elif k == 10:
                d = d[:, :9] * SPAN
            else:
                d = d * EYE_SPAN
            misses.append(d)
    miss = torch.cat(misses).mean(0).tolist()
    torch.set_rng_state(cpu)
    if dev.type == 'cuda':
        torch.cuda.set_rng_state(gpu, dev)
    return dict(zip(LABELS[k], [round(v, 4) for v in miss]))


def main():
    args = options()
    torch.manual_seed(7)
    dev = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    global BANK, OWN, HAIRSET, MYHAIR
    cache = os.path.dirname(os.path.abspath(args.faces))
    hands, masks = hand_bank(cache)
    BANK = (torch.tensor(hands, device=dev).float()[:, None] / 255.0,
            torch.tensor(masks, device=dev).float()[:, None] / 255.0)
    print(f'{len(hands)} hands', flush=True)
    own = own_bank(os.path.expanduser('~/.cache/hyperspace'))
    if own is not None:
        OWN = torch.tensor(own, device=dev).float()[:, None] / 255.0
        print(f'{len(own)} user camera frames (hands, hair)',
              flush=True)
    root = unpack(args.faces)
    hair, hmask, heyes = hair_bank(root, cache)
    HAIRSET = (torch.tensor(hair, device=dev).float()[:, None] / 255.0,
               torch.tensor(hmask, device=dev).float()[:, None] / 255.0,
               torch.tensor(heyes, device=dev))
    print(f'{len(hair)} hairs', flush=True)
    mh = own_bank(os.path.expanduser('~/.cache/hyperspace'), 'myhair')
    if mh is not None:
        MYHAIR = torch.tensor(mh, device=dev).float()[:, None] / 255.0
        print(f'{len(mh)} user camera frames with hair', flush=True)
    frames, pts, turns = load(root)
    pose = poses(root)
    n = max(1, int(len(frames) * args.share))
    frames, pts, pose = frames[:n], pts[:n], pose[:n]
    held = np.arange(n) >= int(n * (1 - HELD))
    # pose bins: BINS steps of up and down by BINS of left and right,
    # even steps over all but the 1% at each end
    edges = [np.linspace(*np.quantile(pose[:, k], [0.01, 0.99]),
                         BINS + 1)[1:-1] for k in (0, 1)]
    row = np.digitize(pose[:, 0], edges[0])
    col = np.digitize(pose[:, 1], edges[1])
    groups, shares, test, cells = [], [], [], []
    for r in range(BINS):
        for c in range(BINS):
            sel = (row == r) & (col == c)
            if (sel & ~held).sum() >= 10 and (sel & held).sum() >= 2:
                groups.append(torch.tensor(np.nonzero(sel & ~held)[0]))
                test.append(torch.tensor(np.nonzero(sel & held)[0]))
                cells.append((r, c))
                # looking up or down most: PITCH_WEIGHT shares
                shares.append(PITCH_WEIGHT if r in (0, BINS - 1)
                              else 1)
    cells = np.asarray(cells)
    mid = BINS // 2
    names = {'up': cells[:, 0] == 0, 'down': cells[:, 0] == BINS - 1,
             'sides': (cells[:, 1] == 0) | (cells[:, 1] == BINS - 1),
             'front': (abs(cells[:, 0] - mid) <= 1)
             & (abs(cells[:, 1] - mid) <= 1)}
    names = {k: torch.tensor(v) for k, v in names.items()}
    up_down = sum(1 for w in shares if w > 1)
    print(f'{n} faces in {len(groups)} pose bins of {BINS * BINS}, '
          f'{up_down} looking up or down most at {PITCH_WEIGHT}x',
          flush=True)
    # his marked frames: a bin of their own (the same share of an
    # epoch as each pose bin), their test kept apart
    own_hold, own_train = own_split(cache)
    of, op = own_rows(own_train)
    # a twelfth number: 1 his own frame, 0 a synthetic face
    pts = np.concatenate([pts, np.zeros((len(pts), 1), np.float32)], 1)
    if of is not None:
        at = len(frames)
        frames = np.concatenate([frames, of])
        pts = np.concatenate([pts, op])
        groups.append(torch.arange(at, at + len(of)))
        # his frames: OWN_SHARE of every epoch, whatever the bins weigh
        bins = sum(shares)
        shares.append(bins * OWN_SHARE / (1 - OWN_SHARE))
    print(f'user marked frames: {len(own_train)} train, '
          f'{len(own_hold)} test, '
          f'{OWN_SHARE:.0%} of every epoch', flush=True)
    frames = torch.tensor(frames)
    if dev.type == 'cuda':
        frames = frames.pin_memory()
    pts = torch.tensor(pts)
    os.makedirs(args.out, exist_ok=True)
    own = own_test(own_hold)
    find, fe = fit(FindNet(), find_batch, frames, pts, groups, shares,
                   test,
                   names, args, 'find', dev,
                   (lambda m: own_score(own, m, None, dev)) if own
                   else None)
    target, te = fit(FaceNet(SIDE), target_batch, frames, pts, groups,
                     shares, test, names, args, 'target', dev,
                     (lambda m: own_score(own, find, m, dev)) if own
                     else None)
    eye, ee = fit(EyeNet(), eye_batch, frames, pts, groups, shares,
                  test, names, args, 'eye', dev,
                  (lambda m: own_score(own, find, target, dev, m))
                  if own else None)
    for name, model, ex in (
            ('find', find, torch.zeros(1, 1, FIND, FIND)),
            ('target', target, torch.zeros(1, 1, SIDE, SIDE)),
            ('eye', eye, torch.zeros(1, 1, EYE, EYE))):
        model = model.cpu()
        torch.save(model.state_dict(),
                   os.path.join(args.out, f'{name}.pt'))
        torch.jit.trace(model, ex).save(
            os.path.join(args.out, f'{name}.ptc'))
    with open(os.path.join(args.out, 'base.txt'), 'w') as f:
        f.write(f'FaceSynthetics, {n} faces, {HELD:.0%} held out\n'
                f'{args.epochs} epochs of {args.epoch_size} pictures\n'
                'mean error over the outputs, share of the corner '
                'gap:\n'
                f'find {FIND}x{FIND} {fe:.4f}\n'
                f'target {SIDE}x{SIDE} {te:.4f}\n'
                f'eye {EYE}x{EYE} {ee:.4f}\n')
    print(f'base: find {fe}, target {te}, eye {ee}', flush=True)


if __name__ == '__main__':
    main()
