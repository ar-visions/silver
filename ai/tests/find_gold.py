# golden files for grad's find ops: python3 ai/tests/find_gold.py <dir>
# then GRAD_GOLD=<dir> silver --test ai (t_grad_find_ops reads them)
import sys, math, torch, numpy as np
import torch.nn.functional as F
out = sys.argv[1]
torch.manual_seed(11)


def put(name, t):
    t.detach().float().contiguous().numpy().astype(np.float32).tofile(
        f'{out}/{name}.f32')


def grads(name, y, *ins):
    dy = torch.randn_like(y)
    y.backward(dy)
    put(f'{name}_y', y)
    put(f'{name}_dy', dy)
    for k, t in enumerate(ins):
        put(f'{name}_d{k}', t.grad)


# conv2d, stride 2, padding 1, 3x3, no bias
x = torch.randn(2, 4, 9, 9, requires_grad=True)
w = torch.randn(6, 4, 3, 3, requires_grad=True)
put('cs_x', x)
put('cs_w', w)
grads('cs', F.conv2d(x, w, None, 2, 1), x, w)

# conv2d, stride 1, padding 2, 5x5, with bias
x = torch.randn(2, 3, 7, 8, requires_grad=True)
w = torch.randn(5, 3, 5, 5, requires_grad=True)
b = torch.randn(5, requires_grad=True)
put('c5_x', x)
put('c5_w', w)
put('c5_b', b)
grads('c5', F.conv2d(x, w, b, 1, 2), x, w, b)

# 1x1, stride 2
x = torch.randn(2, 4, 6, 6, requires_grad=True)
w = torch.randn(3, 4, 1, 1, requires_grad=True)
put('c1_x', x)
put('c1_w', w)
grads('c1', F.conv2d(x, w, None, 2, 0), x, w)

# GroupNorm, 8 groups
x = torch.randn(2, 16, 5, 6, requires_grad=True)
g = torch.randn(16, requires_grad=True)
b = torch.randn(16, requires_grad=True)
put('gn_x', x)
put('gn_g', g)
put('gn_b', b)
grads('gn', F.group_norm(x, 8, g, b, 1e-5), x, g, b)

# SiLU
x = torch.randn(2, 3, 4, 5, requires_grad=True) * 3
x.retain_grad()
put('si_x', x)
grads('si', F.silu(x), x)

# 2x bilinear
x = torch.randn(2, 3, 4, 5, requires_grad=True)
put('up_x', x)
grads('up', F.interpolate(x, scale_factor=2, mode='bilinear',
                          align_corners=False), x)

# softmax at a learned temperature exp(lt), held to 1 .. 100
x = torch.randn(3, 20, requires_grad=True)
lt = torch.tensor([math.log(10.0)], requires_grad=True)
put('st_x', x)
put('st_lt', lt)
grads('st', torch.softmax(x * lt.exp().clamp(1.0, 100.0), 1), x, lt)

# each row's mean
x = torch.randn(6, 7, requires_grad=True)
put('rm_x', x)
grads('rm', x.mean(1), x)
# smooth L1 (beta 0.02), mean
x = torch.randn(5, 4, requires_grad=True) * 0.05
x.retain_grad()
y = torch.randn(5, 4) * 0.05
put('sl_x', x)
put('sl_t', y)
grads('sl', F.smooth_l1_loss(x, y, beta=0.02), x)

# cross entropy against soft targets, mean over rows
x = torch.randn(3, 12, requires_grad=True)
t = torch.softmax(torch.randn(3, 12) * 2, 1)
put('ce_x', x)
put('ce_t', t)
grads('ce', -(t * torch.log_softmax(x, 1)).sum(1).mean(), x)

# batch norm, training: the batch's statistics, running ones moved
x = torch.randn(4, 6, 5, 5, requires_grad=True)
bn = torch.nn.BatchNorm2d(6)
with torch.no_grad():
    bn.weight.normal_()
    bn.bias.normal_()
    bn.running_mean.normal_()
    bn.running_var.uniform_(0.5, 1.5)
put('bn_x', x)
put('bn_g', bn.weight)
put('bn_b', bn.bias)
put('bn_rs', torch.cat([bn.running_mean, bn.running_var]))
bn.train()
grads('bn', bn(x), x, bn.weight, bn.bias)
put('bn_rs2', torch.cat([bn.running_mean, bn.running_var]))

# 2x2 max pooling
x = torch.randn(2, 3, 6, 8, requires_grad=True)
put('mp_x', x)
grads('mp', F.max_pool2d(x, 2), x)
print('written to', out)

# the trained find on frames (find.pt given second): its input at
# 64 x 64 (0 .. 1) and pytorch's x, y, gap a frame
if len(sys.argv) > 2:
    import glob, os
    from PIL import Image
    sys.path.insert(0, os.path.join(os.path.dirname(
        os.path.abspath(__file__)), '..', '..', 'hyperspace'))
    import train_eyes as te
    net = te.FindNet()
    net.load_state_dict(torch.load(sys.argv[2], map_location='cpu'))
    net.eval()
    st = os.path.expanduser('/src/silver/install/home/hyperspace/'
                            '.local/state/hyperspace/gaze')
    fs = sorted(glob.glob(st + '/*-camera*.png'))[::2000][:5]
    xs = []
    for f in fs:
        a = np.asarray(Image.open(f).convert('L'), np.float32) / 255
        full = torch.tensor(a)[None, None]
        xs.append(F.interpolate(full, (te.FIND, te.FIND), mode='area'))
    x = torch.cat(xs)
    with torch.no_grad():
        y = net(x)
    put('fd_in', x)
    put('fd_out', y)
    open(f'{out}/fd.path', 'w').write(sys.argv[2])
    print(f'find on {len(fs)} frames written')
