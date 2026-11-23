# golden files for hyperspace's training pictures in silver
# (ai/HyperData.ag): python3 ai/tests/data_gold.py <dir>, then
# HYPER_DATA=<dir> silver --test ai. Every random draw torch makes
# is kept in order (draws.f64, and each draw's count in draws.n),
# so silver takes the same ones and must make the same pictures
import sys, os, zipfile, torch, numpy as np
import torch.nn.functional as F
from PIL import Image
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(here, '..', '..', 'hyperspace'))
import train_eyes as te
out = sys.argv[1]
os.makedirs(out, exist_ok=True)
torch.manual_seed(3)
draws = []
# off while nets are made (their weights' draws are not the data's)
recording = [True]


def kept(t):
    if recording[0]:
        draws.append(t.detach().flatten().double().numpy())
    return t


def record(fn):
    def call(*a, **k):
        return kept(fn(*a, **k))
    return call


for nm in ['rand', 'randn', 'randint', 'randn_like', 'rand_like',
           'randperm']:
    setattr(torch, nm, record(getattr(torch, nm)))
plain_uniform = torch.Tensor.uniform_
torch.Tensor.uniform_ = lambda t, a=0.0, b=1.0, **k: kept(
    plain_uniform(t, a, b, **k))


def put(name, t):
    np.asarray(t, np.float32).tofile(f'{out}/{name}.f32')


# a few of the cached frames, read in place (the cache is 6 GB)
def cached(n):
    path = os.path.expanduser('~/.cache/hyperspace/synth-256.npz')
    z = zipfile.ZipFile(path)
    got = {}
    for nm in ['frames', 'pts']:
        info = z.getinfo(f'{nm}.npy')
        with open(path, 'rb') as f:
            f.seek(info.header_offset)
            head = f.read(30)
            skip = 30 + int.from_bytes(head[26:28], 'little') \
                + int.from_bytes(head[28:30], 'little')
            f.seek(info.header_offset + skip)
            v = np.lib.format.read_magic(f)
            shape, _, dt = np.lib.format._read_array_header(f, v)
            row = int(np.prod(shape[1:]))
            got[nm] = np.frombuffer(f.read(row * n * dt.itemsize),
                                    dt).reshape((n,) + shape[1:])
    return got['frames'].copy(), te.sized(got['pts'])


frames, pts = cached(32)
# the twelfth number: two frames taken as his own (never painted)
pts = np.concatenate([pts, np.zeros((len(pts), 1), np.float32)], 1)
pts[[3, 17], 11] = 1.0
np.asarray(frames, np.uint8).tofile(f'{out}/frames.u8')
put('pts', pts)
fr = torch.tensor(frames)
pt = torch.tensor(pts)

# the crop alone (find's, before its painting), plain and trained
plain_paint = te.paint
te.paint = lambda win, q, p, aug: win
for aug in [False, True]:
    win, y = te.find_batch(fr, pt, aug)
    tag = 'fa' if aug else 'fp'
    put(f'{tag}_win', win)
    put(f'{tag}_y', y)
te.paint = plain_paint

# small banks (randint's draws are kept, so any size does)
hands = np.load(os.path.expanduser('~/.cache/hyperspace/hands-128.npz'))
hair = np.load(os.path.expanduser('~/.cache/hyperspace/hair-128.npz'))
own = te.own_bank(os.path.expanduser('~/.cache/hyperspace'))
mine = te.own_bank(os.path.expanduser('~/.cache/hyperspace'), 'myhair')
bank = {'hands': hands['hands'][:12], 'hmasks': hands['masks'][:12],
        'hair': hair['hair'][:12], 'hairm': hair['masks'][:12],
        'own': own[::max(1, len(own) // 12)][:12],
        'myhair': mine[::max(1, len(mine) // 12)][:12]}
for k, v in bank.items():
    np.asarray(v, np.uint8).tofile(f'{out}/bank_{k}.u8')
put('bank_eyes', hair['eyes'][:12])


def grey(a):
    return torch.tensor(a).float()[:, None] / 255.0


te.BANK = (grey(bank['hands']), grey(bank['hmasks']))
te.OWN = grey(bank['own'])
te.HAIRSET = (grey(bank['hair']), grey(bank['hairm']),
              torch.tensor(hair['eyes'][:12]))
te.MYHAIR = grey(bank['myhair'])

# each painting stage's picture, in the order they ran
stages = []


def seen(name):
    fn = getattr(te, name)

    def call(*a, **k):
        got = fn(*a, **k)
        put(f'st{len(stages)}', got)
        stages.append(name)
        return got
    setattr(te, name, call)


for nm in ['paint_hair', 'eye_canvas', 'close_eyes', 'ir_look',
           'cover', 'blow_out', 'surround_dark']:
    seen(nm)
for aug in [False, True]:
    win, y = te.find_batch(fr, pt, aug)
    tag = 'ga' if aug else 'gp'
    put(f'{tag}_win', win)
    put(f'{tag}_y', y)
for fn, nm in [(te.target_batch, 't'), (te.eye_batch, 'e')]:
    for aug in [False, True]:
        win, y = fn(fr, pt, aug)
        tag = nm + ('a' if aug else 'p')
        put(f'{tag}_win', win)
        put(f'{tag}_y', y)
open(f'{out}/stages.txt', 'w').write('\n'.join(stages) + '\n')

# FaceSynthetics' own files read as train_eyes reads them
root = os.path.expanduser('~/.cache/hyperspace/facesynth/dataset_100000')
names = [f'{k:06d}' for k in range(0, 100000, 4111)][:16]
got = [te.decode((f'{root}/{nm}.png', f'{root}/{nm}_ldmks.txt'))
       for nm in names]
np.stack([g[0] for g in got]).astype(np.uint8).tofile(
    f'{out}/dec_frames.u8')
dp = te.sized(np.asarray([g[1] for g in got], np.float32))
put('dec_pts', dp)
put('dec_pose', [te.pose_one(f'{root}/{nm}_ldmks.txt') for nm in names])
open(f'{out}/dec_names.txt', 'w').write('\n'.join(
    f'{root}/{nm}' for nm in names) + '\n')
# his marked frames, split and read as rows
cache = os.path.expanduser('~/.cache/hyperspace')
hold, train = te.own_split(cache)
of, op = te.own_rows(train)
of.astype(np.uint8).tofile(f'{out}/own_frames.u8')
put('own_pts', op)
open(f'{out}/own_names.txt', 'w').write(
    ''.join(f'{nm}\n' for nm, _, _ in hold) + '--\n'
    + ''.join(f'{nm}\n' for nm, _, _ in train))

# the pose bins and an epoch's order (main's, then one_pass)
pose = np.load(os.path.join(cache, 'pose.npy'))
put('pose', pose)
n = len(pose)
held = np.arange(n) >= int(n * (1 - te.HELD))
edges = [np.linspace(*np.quantile(pose[:, k], [0.01, 0.99]),
                     te.BINS + 1)[1:-1] for k in (0, 1)]
row = np.digitize(pose[:, 0], edges[0])
col = np.digitize(pose[:, 1], edges[1])
groups, shares, test = [], [], []
for r in range(te.BINS):
    for c in range(te.BINS):
        sel = (row == r) & (col == c)
        if (sel & ~held).sum() >= 10 and (sel & held).sum() >= 2:
            groups.append(torch.tensor(np.nonzero(sel & ~held)[0]))
            test.append(torch.tensor(np.nonzero(sel & held)[0]))
            shares.append(te.PITCH_WEIGHT if r in (0, te.BINS - 1)
                          else 1)
groups.append(torch.arange(n, n + len(of)))
shares.append(sum(shares) * te.OWN_SHARE / (1 - te.OWN_SHARE))
for nm, gs in [('groups', groups), ('tests', test)]:
    np.asarray([len(g) for g in gs], np.int64).tofile(f'{out}/{nm}.n')
    torch.cat(gs).numpy().astype(np.int64).tofile(f'{out}/{nm}.i64')
put('shares', shares)
order = te.one_pass(groups, shares, 2, 3000)
order.numpy().astype(np.int64).tofile(f'{out}/order.i64')
np.concatenate(draws).tofile(f'{out}/draws.f64')
np.asarray([len(d) for d in draws], np.int64).tofile(f'{out}/draws.n')
draws.clear()

# his whole camera banks, as train_eyes makes them
np.asarray(own, np.uint8).tofile(f'{out}/own_full.u8')
np.asarray(mine, np.uint8).tofile(f'{out}/myhair_full.u8')

# the trained run as the app reads it (export_eyes.py), to check
# silver's own writer
import export_eyes as ex
import train as tr
recording[0] = False
for name, model, side, outputs in (
        ('find', te.FindNet(), te.FIND, 3),
        ('target', tr.FaceNet(te.SIDE), te.SIDE, 10),
        ('eye', te.EyeNet(), te.EYE, 4)):
    model.load_state_dict(torch.load(os.path.join(
        cache, 'runs', 'eyes', f'{name}.pt'), map_location='cpu'))
    model.eval()
    ex.write(model, side, outputs, f'{out}/{name}.hsn')
recording[0] = True

# gaze: a few captures, their points by the trained chain (python's),
# the clip bank, a training batch's windows, the shake
import glob
import train_gaze as tg
recording[0] = False
find, target, eye = tg.nets(os.path.join(cache, 'runs', 'eyes'), 'cpu')
gfr, gpt, gmk, gdots, gfd, gpr = [], [], [], [], [], []
gcr, gto = [], []
caps = sorted(glob.glob(os.path.join(tg.STATE, 'gaze', '*.agi')))
for f in caps[::max(1, len(caps) // 24)][:24]:
    fs, ps, ms = [], [], []
    for cam in (0, 1):
        g = Image.open(f'{f[:-4]}-camera{cam}.png').convert('L')
        a = np.asarray(g, np.float32) / 255
        full = torch.tensor(a)[None, None]
        with torch.no_grad():
            small = F.interpolate(full, (te.FIND, te.FIND), mode='area')
            fo = find(small)[0]
            gfd.append(fo.numpy())
            cx, cy, sc = fo.tolist()
            sv = sc * te.SPAN
            th = torch.tensor([[[sv, 0, cx * 2 - 1], [0, sv, cy * 2 - 1]]],
                              dtype=torch.float32)
            grid = F.affine_grid(th, (1, 1, te.SIDE, te.SIDE),
                                 align_corners=False)
            crop = F.grid_sample(full, grid, align_corners=False)
            gcr.append(crop[0, 0].numpy())
            gto.append(target(crop)[0].numpy())
            q, _ = te.run_pair(find, target, full, 'cpu')
            gpr.append(np.asarray(q))
            q = te.run_eyes(eye, full, q, 'cpu')
            again = te.run_eyes(eye, full, q, 'cpu')
        ms.append(tg.judge(np.asarray(q), np.asarray(again),
                           a.shape[1]))
        ps.append(q)
        fs.append(np.asarray(g, np.uint8))
    gfr.append(np.stack(fs))
    gpt.append(np.stack(ps).astype(np.float32))
    gmk.append(ms)
gfr, gpt = np.stack(gfr), np.stack(gpt)
recording[0] = True
gmk = np.asarray(gmk)
gfr.tofile(f'{out}/gz_frames.u8')
put('gz_pts', gpt)
put('gz_find', np.stack(gfd))
put('gz_pair', np.stack(gpr))
put('gz_crops', np.stack(gcr))
put('gz_tout', np.stack(gto))
put('gz_marks', gmk)
open(f'{out}/gz_size.txt', 'w').write(
    f'{gfr.shape[0]} {gfr.shape[2]} {gfr.shape[3]}\n')
tg.SOUND = torch.tensor((gmk == 0).astype(np.float32))
bank = tg.clip_bank(gfr, gpt, 'cpu')
for k, v in enumerate(bank):
    put(f'gz_bank{k}', v)
take = torch.tensor([3, 0, 7, 7, 15, 22, 11, 5])
eyes, faces, aux = tg.batch(bank, take, True)
for k, v in [('eyes', eyes), ('faces', faces), ('aux', aux),
             ('se', tg.shake(eyes)), ('sf', tg.shake(faces))]:
    put(f'gz_{k}', v)
eyes, faces, aux = tg.batch(bank, take, False)
put('gz_plain_eyes', eyes)
put('gz_plain_aux', aux)
np.concatenate(draws).tofile(f'{out}/gz_draws.f64')
np.asarray([len(d) for d in draws], np.int64).tofile(
    f'{out}/gz_draws.n')
print('written to', out, len(draws), 'draws')
