# a trained run's nets as the app reads them (hyperspace.c): each
# batch norm folded into its conv, the floats in order
import argparse
import os
import struct
import sys

import numpy as np
import torch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from train_eyes import EYE, FIND, SIDE, EyeNet, FindNet  # noqa: E402
from train import FaceNet  # noqa: E402

MAGIC = 0x314E5348


def floats(model):
    out = []
    mods = list(model.trunk)
    for k, m in enumerate(mods):
        if not isinstance(m, torch.nn.Conv2d):
            continue
        bn = mods[k + 1]
        g = bn.weight / torch.sqrt(bn.running_var + bn.eps)
        w = m.weight * g.view(-1, 1, 1, 1)
        b = (m.bias - bn.running_mean) * g + bn.bias
        out += [w.flatten(), b]
    for m in model.head:
        if isinstance(m, torch.nn.Linear):
            out += [m.weight.flatten(), m.bias]
    return torch.cat([t.detach().float() for t in out]).numpy()


# the find net (residual blocks, a heat map): its floats in the
# order hyperspace.c's hs_find_run reads them
FIND_MAGIC = 0x31465348


def find_floats(model):
    out = []

    def conv_norm(conv, norm):
        out.extend([conv.weight, norm.weight, norm.bias])

    def block(k):
        conv_norm(k.body[0], k.body[1])
        conv_norm(k.body[3], k.body[4])
        if not isinstance(k.skip, torch.nn.Identity):
            out.append(k.skip.weight)

    conv_norm(model.stem[0], model.stem[1])
    block(model.full)
    for part in (model.at_half, model.at_quarter, model.at_eighth):
        block(part[0])
        block(part[1])
    out.extend([model.to_quarter.weight, model.to_quarter.bias])
    block(model.refine_quarter)
    out.extend([model.to_half.weight, model.to_half.bias])
    block(model.refine_half)
    out.extend([model.heat.weight, model.heat.bias,
                model.log_temp.view(1)])
    out.extend([model.gap[0].weight, model.gap[0].bias,
                model.gap[2].weight, model.gap[2].bias])
    return torch.cat([t.detach().float().flatten()
                      for t in out]).numpy()


# the gaze net: each encoder's convs (batch norm folded) and dense
# layer, the eye encoder first; the head's layer norm, two layers;
# HSG4, the encoders' first conv kernel, the cameras read, the first
GAZE_MAGIC = 0x34475348


def gaze_floats(model):
    out = []
    for enc in (model.eye, model.face):
        mods = list(enc)
        for k, m in enumerate(mods):
            if isinstance(m, torch.nn.Conv2d):
                bn = mods[k + 1]
                g = bn.weight / torch.sqrt(bn.running_var + bn.eps)
                out += [m.weight * g.view(-1, 1, 1, 1),
                        (m.bias - bn.running_mean) * g + bn.bias]
            elif isinstance(m, torch.nn.Linear):
                out += [m.weight, m.bias]
    norm, first, last = model.head[0], model.head[1], model.head[4]
    out += [norm.weight, norm.bias, first.weight, first.bias,
            last.weight, last.bias]
    return torch.cat([t.detach().float().flatten()
                      for t in out]).numpy()


def write_gaze(model, path):
    with open(path, 'wb') as f:
        k = model.eye[0].kernel_size[0]
        # the cameras it reads: how many, then the first
        cams = getattr(model, 'cams', [0, 1])
        f.write(struct.pack('<4i', GAZE_MAGIC, k, len(cams), cams[0]))
        f.write(gaze_floats(model).astype('<f4').tobytes())


def write(model, side, outputs, path):
    find = isinstance(model, FindNet)
    with open(path, 'wb') as f:
        f.write(struct.pack('<3i', FIND_MAGIC if find else MAGIC,
                            side, outputs))
        data = find_floats(model) if find else floats(model)
        f.write(data.astype('<f4').tobytes())


def main():
    home = os.path.expanduser('~')
    app = os.path.join('/src/silver/install/home/hyperspace',
                       '.local/state/hyperspace/models')
    p = argparse.ArgumentParser()
    runs = os.path.join(home, '.cache/hyperspace/runs')
    p.add_argument('run', nargs='?', default=os.path.join(runs, 'eyes'))
    p.add_argument('--to', default=app)
    a = p.parse_args()
    os.makedirs(a.to, exist_ok=True)
    for name, model, side, outputs in (
            ('find', FindNet(), FIND, 3),
            ('target', FaceNet(SIDE), SIDE, 10),
            ('eye', EyeNet(), EYE, 4)):
        pt = os.path.join(a.run, f'{name}.pt')
        model.load_state_dict(torch.load(pt, map_location='cpu'))
        model.eval()
        write(model, side, outputs, os.path.join(a.to, f'{name}.hsn'))
        print(f'{name}.hsn written to {a.to}')


if __name__ == '__main__':
    main()
