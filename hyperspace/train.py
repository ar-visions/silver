#!/usr/bin/env python3
import argparse
import hashlib
import math
import os
import shlex

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from PIL import Image


def options():
    p = argparse.ArgumentParser()
    p.add_argument('--data')
    p.add_argument('--out')
    p.add_argument('--epochs', type=int, default=40)
    p.add_argument('--batch', type=int, default=64)
    p.add_argument('--size', type=int, default=32)
    p.add_argument('--lr', type=float, default=0.001)
    p.add_argument('--limit', type=int, default=0)
    p.add_argument('--check', action='store_true')
    return p.parse_args()


def read_record(path):
    text = open(path, encoding='utf-8').read()
    words = shlex.split(text)
    out = {}
    for word in words:
        if '=' in word:
            key, value = word.split('=', 1)
            out[key] = value
    legacy = {}
    for line in text.splitlines():
        if ':' not in line:
            continue
        key, value = line.strip().split(':', 1)
        value = value.strip()
        if value:
            legacy[key] = value
    images = []
    for key, value in legacy.items():
        if value.lower().endswith(('png', 'jpg', 'jpeg', 'webp')):
            images.append((key, value))
    if images and 'image0' not in out:
        out['cameras'] = str(len(images))
        for camera, (role, image) in enumerate(images):
            out[f'image{camera}'] = image
            mapping = (
                ('left_oc', 'left_outer'),
                ('right_oc', 'right_outer'),
                ('left_pupil', 'left_pupil'),
                ('right_pupil', 'right_pupil'))
            for old, new in mapping:
                pair = legacy.get(f'{role}_{old}', '').split()
                if len(pair) == 2:
                    out[f'{new}_x{camera}'] = pair[0]
                    out[f'{new}_y{camera}'] = pair[1]
        look = legacy.get('look', '').split()
        if len(look) == 2:
            out['target_x'] = str(float(look[0]) + 0.5)
            out['target_y'] = str(float(look[1]) + 0.5)
        out['source'] = 'user'
    out['_id'] = os.path.splitext(os.path.basename(path))[0]
    return out


def number(record, key, default=-1.0):
    try:
        return float(record.get(key, default))
    except ValueError:
        return default


def image_path(folder, record, camera):
    named = record.get(f'image{camera}')
    if named:
        return os.path.join(folder, named)
    stem = f'{record["_id"]}-camera{camera}'
    for ext in ('png', 'jpg', 'jpeg', 'webp'):
        path = os.path.join(folder, f'{stem}.{ext}')
        if os.path.exists(path):
            return path
    return None


def image_tensor(path, side):
    image = Image.open(path).convert('L')
    image = image.resize((side, side), Image.Resampling.BOX)
    pixels = np.asarray(image, np.float32) / 255.0
    return pixels[None]


def face_label(record, camera):
    names = ('left_outer_x', 'left_outer_y',
             'right_outer_x', 'right_outer_y',
             'left_pupil_x', 'left_pupil_y',
             'right_pupil_x', 'right_pupil_y')
    points = [number(record, f'{name}{camera}') for name in names]
    if min(points) < 0.0 or max(points) > 1.0:
        return None
    dx = points[0] - points[2]
    dy = points[1] - points[3]
    scale = math.sqrt(dx * dx + dy * dy)
    return np.asarray(points + [scale, 1.0], np.float32)


def load(folder, side, limit=0):
    records = []
    face_images = []
    face_labels = []
    for name in sorted(os.listdir(folder)):
        if not name.endswith('.agi'):
            continue
        record = read_record(os.path.join(folder, name))
        views = []
        labels = []
        cameras = int(number(record, 'cameras', 1))
        for camera in range(max(1, cameras)):
            path = image_path(folder, record, camera)
            if not path:
                continue
            pixels = image_tensor(path, side)
            label = face_label(record, camera)
            views.append(pixels)
            labels.append(label)
            if label is not None:
                face_images.append(pixels)
                face_labels.append(label)
        if views:
            record['_views'] = views
            record['_faces'] = labels
            records.append(record)
            if limit > 0 and len(records) >= limit:
                break
    fx = np.asarray(face_images, np.float32)
    fy = np.asarray(face_labels, np.float32)
    return records, fx, fy


def conv(ci, co, kernel=3, stride=1):
    return nn.Sequential(
        nn.Conv2d(ci, co, kernel, stride=stride,
                  padding=kernel // 2, bias=False),
        nn.BatchNorm2d(co), nn.ReLU())


def normalize(x):
    mean = x.mean(dim=(1, 2, 3), keepdim=True)
    std = x.std(dim=(1, 2, 3), keepdim=True).clamp(min=1e-4)
    return (x - mean) / std


def soft_point(heat, temp, axis):
    batch, count, side, _ = heat.shape
    flat = heat.reshape(batch, count, side * side)
    prob = torch.softmax(flat * temp, 2)
    prob = prob.reshape(batch, count, side, side)
    xs = (prob.sum(2) * axis).sum(2)
    ys = (prob.sum(3) * axis).sum(2)
    return xs, ys


class Encoder(nn.Module):
    def __init__(self, side, points=8, width=64):
        super().__init__()
        self.trunk = nn.Sequential(
            conv(1, 32, 5), conv(32, width), conv(width, width))
        self.heat = nn.Conv2d(width, points, 1)
        self.temp = nn.Parameter(torch.tensor(8.0))
        self.register_buffer('axis', torch.linspace(0.0, 1.0, side))
        self.app = nn.Sequential(
            conv(width, width, stride=2),
            conv(width, width * 2, stride=2),
            conv(width * 2, width * 2, stride=2),
            nn.Flatten())
        self.features = points * 2 + width * 2 * (side // 8) ** 2

    def forward(self, x):
        trunk = self.trunk(normalize(x))
        xs, ys = soft_point(self.heat(trunk), self.temp, self.axis)
        return torch.cat([xs, ys, self.app(trunk)], 1)


# four stages, each two 3x3 convolutions (batch norm, ReLU) and a
# 2x2 max pool: 64 -> 4, each feature then sees the whole picture;
# a fully connected head: side x side grey in, `outputs` numbers out
# the stage networks' first conv kernel (3 or 5; the app's C code
# reads either)
FIRST = 5


# first: the first conv's kernel (5 sees an edge; 3 after it)
def stage(cin, cout, first=3):
    return [nn.Conv2d(cin, cout, first, padding=first // 2),
            nn.BatchNorm2d(cout),
            nn.ReLU(), nn.Conv2d(cout, cout, 3, padding=1),
            nn.BatchNorm2d(cout), nn.ReLU(), nn.MaxPool2d(2)]


class StageConv(nn.Module):
    def __init__(self, side, outputs):
        super().__init__()
        self.trunk = nn.Sequential(
            *stage(1, 16, FIRST), *stage(16, 32), *stage(32, 64),
            *stage(64, 128), nn.Flatten())
        self.head = nn.Sequential(
            nn.Linear(128 * (side // 16) ** 2, 128), nn.ReLU(),
            nn.Linear(128, outputs))

    def forward(self, x):
        return self.head(self.trunk(normalize(x)))


# target: in the face's square, both outer eye corners and both
# eye centers (x, y each), the scale, the visible flag
class FaceNet(StageConv):
    def __init__(self, side):
        super().__init__(side, 10)


class ViewNet(nn.Module):
    def __init__(self, side, outputs):
        super().__init__()
        self.encoder = Encoder(side)
        width = self.encoder.features + 10
        self.head = nn.Sequential(
            nn.Linear(width, 128), nn.ReLU(),
            nn.Linear(128, 128), nn.ReLU(),
            nn.Linear(128, outputs))

    def features(self, images, face):
        batch, views, _, side, _ = images.shape
        flat = images.reshape(batch * views, 1, side, side)
        encoded = self.encoder(flat).reshape(batch, views, -1)
        joined = torch.cat([encoded, face], 2)
        visible = face[:, :, 9:10].clamp(0.0, 1.0)
        total = visible.sum(1).clamp(min=1.0)
        return (joined * visible).sum(1) / total

    def forward(self, images, face):
        return self.head(self.features(images, face))


def split(ids):
    train = []
    test = []
    for index, item in enumerate(ids):
        digest = hashlib.sha256(str(item).encode()).digest()
        (test if digest[0] < 51 else train).append(index)
    if not train and test:
        train.append(test.pop())
    if not test and len(train) > 1:
        test.append(train.pop())
    return train, test


def fit(model, x, y, ids, args, name):
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    model.to(device)
    x = [torch.tensor(value).to(device) for value in x]
    y = torch.tensor(y).to(device)
    train, test = split(ids)
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr)
    best = None
    best_state = None
    for epoch in range(args.epochs):
        model.train()
        order = np.random.permutation(train)
        for start in range(0, len(order), args.batch):
            take = order[start:start + args.batch]
            pred = model(*[value[take] for value in x])
            loss = F.mse_loss(pred, y[take])
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()
        model.eval()
        with torch.no_grad():
            use = test or train
            error = F.mse_loss(
                model(*[value[use] for value in x]), y[use]).item()
        print(f'{name} {epoch + 1}/{args.epochs} error {error:.6f}')
        if best is None or error < best:
            best = error
            best_state = {
                key: value.detach().cpu().clone()
                for key, value in model.state_dict().items()}
    model.load_state_dict(best_state)
    return model.cpu().eval()


def annotate(records, face_model, side, max_views):
    rows = []
    for record in records:
        images = np.zeros((max_views, 1, side, side), np.float32)
        faces = np.zeros((max_views, 10), np.float32)
        for camera, pixels in enumerate(record['_views'][:max_views]):
            images[camera] = pixels
            label = record['_faces'][camera]
            if label is None:
                with torch.no_grad():
                    sample = torch.tensor(pixels[None])
                    label = face_model(sample)[0].numpy()
                    label[9] = 1.0
            faces[camera] = label
        rows.append((record, images, faces))
    return rows


def look_data(rows):
    images, faces, labels, ids = [], [], [], []
    for record, view, face in rows:
        x = number(record, 'target_x')
        y = number(record, 'target_y')
        if min(x, y) < 0.0 or max(x, y) > 1.0:
            continue
        images.append(view)
        faces.append(face)
        labels.append([x, y])
        ids.append(record['_id'])
    return images, faces, labels, ids


def gesture_data(rows):
    names = sorted({record.get('gesture', 'none')
                    for record, _, _ in rows
                    if record.get('gesture', 'none') != 'none'})
    images, faces, labels, ids = [], [], [], []
    for record, view, face in rows:
        name = record.get('gesture', 'none')
        if name != 'none' and name not in names:
            continue
        hand = [number(record, key) for key in (
            'left_hand_x', 'left_hand_y', 'left_hand_scale',
            'right_hand_x', 'right_hand_y', 'right_hand_scale')]
        scale = number(record, 'gesture_scale', 1.0)
        one_hot = [1.0 if item == name else 0.0 for item in names]
        images.append(view)
        faces.append(face)
        labels.append(hand + [scale] + one_hot)
        ids.append(record['_id'])
    return names, images, faces, labels, ids


def export(model, args, name, example):
    os.makedirs(args.out, exist_ok=True)
    torch.save(model.state_dict(), os.path.join(args.out, f'{name}.pt'))
    traced = torch.jit.trace(model, example)
    traced.save(os.path.join(args.out, f'{name}.ptc'))


def check(args):
    face = FaceNet(args.size)
    image = torch.zeros(2, 1, args.size, args.size)
    assert face(image).shape == (2, 10)
    look = ViewNet(args.size, 2)
    views = torch.zeros(2, 3, 1, args.size, args.size)
    labels = torch.zeros(2, 3, 10)
    labels[:, :, 9] = 1.0
    assert look(views, labels).shape == (2, 2)
    print('hyperspace training core: pass')


def main():
    args = options()
    torch.manual_seed(7)
    np.random.seed(7)
    if args.check:
        check(args)
        return
    if not args.data or not args.out:
        raise SystemExit('--data and --out are required')
    records, face_x, face_y = load(
        args.data, args.size, args.limit)
    if len(face_x) < 2:
        raise SystemExit('need two reviewed faces with OC plots')
    face_ids = list(range(len(face_x)))
    face = fit(FaceNet(args.size), [face_x], face_y,
               face_ids, args, 'face')
    export(face, args, 'face',
           (torch.zeros(1, 1, args.size, args.size),))
    max_views = max(len(record['_views']) for record in records)
    rows = annotate(records, face, args.size, max_views)
    images, faces, labels, ids = look_data(rows)
    if len(images) >= 2:
        look = fit(ViewNet(args.size, 2),
                   [np.asarray(images), np.asarray(faces)],
                   labels, ids, args, 'look')
        example = (
            torch.zeros(1, max_views, 1, args.size, args.size),
            torch.zeros(1, max_views, 10))
        export(look, args, 'look', example)
    names, images, faces, labels, ids = gesture_data(rows)
    if len(images) >= 2:
        gesture = fit(ViewNet(args.size, 7 + len(names)),
                      [np.asarray(images), np.asarray(faces)],
                      labels, ids, args, 'gesture')
        example = (
            torch.zeros(1, max_views, 1, args.size, args.size),
            torch.zeros(1, max_views, 10))
        export(gesture, args, 'gesture', example)
        with open(os.path.join(args.out, 'gestures.txt'), 'w',
                  encoding='utf-8') as handle:
            handle.write('\n'.join(names) + '\n')
    print('hyperspace real-image training complete')


if __name__ == '__main__':
    main()
