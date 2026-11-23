# golden files for hyperspace's networks on grad (ai/Hyper.ag):
# python3 ai/tests/hyper_gold.py <dir>, then HYPER_GOLD=<dir> silver
# --test ai. Each net: its weights (<net>.pt), one batch, the loss,
# every weight's gradient by name, the running statistics after
import sys, os, torch, numpy as np
import torch.nn.functional as F
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(here, '..', '..', 'hyperspace'))
import train, train_eyes as te, train_gaze as tg
out = sys.argv[1]
os.makedirs(out, exist_ok=True)
torch.manual_seed(5)


def put(name, t):
    t.detach().float().contiguous().numpy().astype(np.float32).tofile(
        f'{out}/{name}.f32')


# random running statistics, so a pass that reads them shows it
def shake(net):
    for m in net.modules():
        if isinstance(m, torch.nn.BatchNorm2d):
            with torch.no_grad():
                m.running_mean.normal_(0, 0.2)
                m.running_var.uniform_(0.5, 1.5)


# the weights are written before the batch (training moves the
# running statistics)
def save(tag, net, loss, parts):
    loss.backward()
    put(f'{tag}_loss', torch.stack(parts))
    names = []
    for nm, p in net.named_parameters():
        if p.grad is None:
            continue
        put(f'{tag}_g{len(names)}', p.grad)
        names.append(nm)
    open(f'{out}/{tag}.names', 'w').write('\n'.join(names) + '\n')
    rs = [torch.cat([m.running_mean, m.running_var])
          for m in net.modules() if isinstance(m, torch.nn.BatchNorm2d)]
    if rs:
        put(f'{tag}_rs', torch.cat(rs))


# target's stage net: 64 x 64, 10 outputs
net = train.StageConv(64, 10)
shake(net)
torch.save(net.state_dict(), f'{out}/stage.pt')
net.train()
x = torch.rand(4, 1, 64, 64)
y = torch.rand(4, 10)
put('stage_x', x)
put('stage_t', y)
o = net(x)
put('stage_y', o)
loss = F.smooth_l1_loss(o, y, beta=0.01)
save('stage', net, loss, [loss])

# find: its three losses, at the weights training uses
net = te.FindNet()
net.train()
torch.save(net.state_dict(), f'{out}/find.pt')
x = torch.rand(3, 1, te.FIND, te.FIND)
y = torch.cat([torch.rand(3, 2) * 0.6 + 0.2, torch.rand(3, 1) * 0.3
               + 0.1], 1)
put('find_x', x)
put('find_t', y)
o, heat = net.locate(x)
put('find_y', o)
gx = net.axis.view(1, 1, 1, -1)
gy = net.axis.view(1, 1, -1, 1)
sg = 1.5 / heat.shape[-1]
d2 = (gx - y[:, 0].view(-1, 1, 1, 1)) ** 2 \
    + (gy - y[:, 1].view(-1, 1, 1, 1)) ** 2
bump = torch.softmax((-d2 / (2 * sg * sg)).flatten(1), 1)
hl = -(bump * torch.log_softmax(heat.flatten(1), 1)).sum(1).mean()
pl = F.smooth_l1_loss(o[:, :2], y[:, :2], beta=0.01)
gl = F.smooth_l1_loss(o[:, 2].clamp(min=1e-4).log(),
                      y[:, 2].clamp(min=1e-4).log(), beta=0.05)
save('find', net, pl + gl * 0.1 + hl * te.HEAT_WEIGHT, [pl, gl, hl])

# gaze: dropout off (its draws cannot be matched)
net = tg.GazeNet([0, 1])
shake(net)
net.head[3].p = 0.0
net.train()
torch.save(net.state_dict(), f'{out}/gaze.pt')
e = torch.rand(4, 4, tg.EYE_CLIP, tg.EYE_CLIP)
f = torch.rand(4, 2, tg.CLIP, tg.CLIP)
a = torch.rand(4, 38) - 0.5
d = torch.rand(4, 2) - 0.5
for k, t in [('e', e), ('f', f), ('a', a), ('t', d)]:
    put(f'gaze_{k}', t)
o = net(e, f, a)
put('gaze_y', o)
loss = F.smooth_l1_loss(o, d, beta=0.02)
save('gaze', net, loss, [loss])
print('written to', out)
