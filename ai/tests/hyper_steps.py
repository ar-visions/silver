# golden runs of steps for ai/Hyper.ag: python3 ai/tests/hyper_steps.py
# <dir> [steps]: each net trained on a few fixed batches, cycled, by
# its script's optimizer and schedule; every step's loss before its
# update, in float64 (the exact answer, near enough) and in float32
# (pytorch's own rounding). HYPER_GOLD=<dir> silver --test ai checks
import sys, os, math, torch, numpy as np
import torch.nn.functional as F
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(here, '..', '..', 'hyperspace'))
import train, train_eyes as te, train_gaze as tg
out = sys.argv[1]
steps = int(sys.argv[2]) if len(sys.argv) > 2 else 200
kinds = 4
os.makedirs(out, exist_ok=True)
torch.manual_seed(9)


def put(name, t):
    t.detach().float().contiguous().numpy().astype(np.float32).tofile(
        f'{out}/{name}.f32')


put('s_count', torch.tensor([float(steps), float(kinds)]))


def run(tag, net, make_opt, make_sched, batches, loss_of, clip=0.0):
    torch.save(net.state_dict(), f'{out}/s_{tag}.pt')
    first = {k: v.clone() for k, v in net.state_dict().items()}
    for dt, end in [(torch.float64, ''), (torch.float32, '32')]:
        net.load_state_dict(first)
        net.to(dt)
        net.train()
        opt = make_opt(net)
        sched = make_sched(opt)
        lost = []
        for s in range(steps):
            b = [t.to(dt) for t in batches[s % kinds]]
            loss = loss_of(net, b)
            opt.zero_grad()
            loss.backward()
            if clip > 0:
                torch.nn.utils.clip_grad_norm_(net.parameters(), clip)
            opt.step()
            sched.step()
            lost.append(loss.item())
        put(f's_{tag}_loss{end}', torch.tensor(lost))
        print(tag, dt, 'first', lost[0], 'last', lost[-1])
        net.float()


net = train.StageConv(64, 10)
bs = []
for k in range(kinds):
    x = torch.rand(8, 1, 64, 64)
    y = torch.rand(8, 10)
    put(f's_stage_x{k}', x)
    put(f's_stage_t{k}', y)
    bs.append((x, y))
cyc = lambda o: torch.optim.lr_scheduler.OneCycleLR(o, te.LR,
                                                   total_steps=steps)
run('stage', net, lambda n: torch.optim.AdamW(n.parameters(), lr=te.LR),
    cyc, bs,
    lambda n, b: F.smooth_l1_loss(n(b[0]), b[1], beta=0.01))

net = te.FindNet()
bs = []
for k in range(kinds):
    x = torch.rand(6, 1, te.FIND, te.FIND)
    y = torch.cat([torch.rand(6, 2) * 0.6 + 0.2,
                   torch.rand(6, 1) * 0.3 + 0.1], 1)
    put(f's_find_x{k}', x)
    put(f's_find_t{k}', y)
    bs.append((x, y))
run('find', net, lambda n: torch.optim.AdamW(n.parameters(), lr=te.LR),
    cyc, bs,
    lambda n, b: te.find_loss(n, b[0], b[1]))

net = tg.GazeNet([0, 1])
net.head[3].p = 0.0
bs = []
for k in range(kinds):
    b = (torch.rand(8, 4, 32, 32), torch.rand(8, 2, 32, 32),
         torch.rand(8, 38) - 0.5, torch.rand(8, 2) - 0.5)
    for nm, t in zip('efat', b):
        put(f's_gaze_{nm}{k}', t)
    bs.append(b)
run('gaze', net, lambda n: torch.optim.AdamW(n.parameters(), lr=3e-4,
                                            weight_decay=1e-2),
    lambda o: torch.optim.lr_scheduler.CosineAnnealingLR(o, steps), bs,
    lambda n, b: F.smooth_l1_loss(n(b[0], b[1], b[2]), b[3], beta=0.02),
    clip=1.0)
