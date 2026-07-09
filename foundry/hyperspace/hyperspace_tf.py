#!/usr/bin/env python3
# TensorFlow reference for hyperspace --train: same nets, data, labels, args.
import argparse, os, re, sys, warnings
os.environ.setdefault('TF_CPP_MIN_LOG_LEVEL', '3')
warnings.filterwarnings('ignore')
import numpy as np

def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument('--session',   default='basic')
    p.add_argument('--epochs',    type=int,   default=4)
    p.add_argument('--optimizer', default='adam', choices=['adam', 'sgd'])
    p.add_argument('--lr',        type=float, default=0.001)
    p.add_argument('--batch',     type=int,   default=1)
    p.add_argument('--train',     action='store_true')  # parity with silver CLI
    p.add_argument('--verbose',   action='store_true')
    p.add_argument('--size',      type=int,   default=32)
    p.add_argument('--mirror',    type=int, default=1)
    p.add_argument('--model',     default='all', choices=[
        'all', 'target', 'look', 'refine',
        'look_0_0', 'look_0_1', 'look_1_0', 'look_1_1', 'look_2_0', 'look_2_1'])
    p.add_argument('--cams',      type=int, default=1)   # 1 = top only; 2 = top+bot
    p.add_argument('--preview',   action='store_true')   # dump look inputs as pngs
    p.add_argument('--alternate', type=int, default=0)   # 1 = old vision-train track net
    p.add_argument('--export',    default='')            # write silver .bin (implies silver-parity net)
    p.add_argument('--look_rot',  type=float, default=0.0)  # unused: look rotation removed
    p.add_argument('--export_tflite', default='')           # write .tflite for the app's tflite-c runtime
    p.add_argument('--seed',      type=int, default=1234)   # split/augment seed; 0 = random
    p.add_argument('--zero_aux',  type=int, default=0)      # 1 = feed zeros as aux (dead-input test)
    p.add_argument('--aux_dense', type=int, default=0)      # widen aux to N features before concat
    p.add_argument('--validate',  action='store_true')      # dump raw tensor values entering fit()
    p.add_argument('--film',      type=int, default=1)      # aux modulates image features (0 = plain concat)
    return p.parse_args()

args = parse_args()

def read_pair(agi_text, key):
    # scale entries carry a single value; y is optional
    m = re.search(rf'{re.escape(key)}:\s*(-?[\d.]+)(?:[ \t]+(-?[\d.]+))?', agi_text)
    if not m:
        return (-1.0, -1.0)
    return (float(m.group(1)), float(m.group(2)) if m.group(2) else 0.0)

def crop_tensor(gray, cx, cy, side):
    # square crop -> 32x32 gray, area-average (sync with silver crop_tensor)
    h, w = gray.shape
    img_w   = float(w)
    global args
    size = args.size
    side_px = side * img_w if side > 0.0 else img_w
    x0      = cx * img_w - side_px * 0.5 if side > 0.0 else 0.0
    y0      = cy * img_w - side_px * 0.5 if side > 0.0 else 0.0
    step    = side_px / size
    g   = gray.astype(np.float32)
    out = np.zeros((size, size), np.float32)
    for oy in range(size):
        ys, ye = int(y0 + oy * step), int(y0 + (oy + 1) * step)
        if ye <= ys: ye = ys + 1
        yy = np.clip(np.arange(ys, ye), 0, h - 1)
        for ox in range(size):
            xs, xe = int(x0 + ox * step), int(x0 + (ox + 1) * step)
            if xe <= xs: xe = xs + 1
            xx = np.clip(np.arange(xs, xe), 0, w - 1)
            out[oy, ox] = g[np.ix_(yy, xx)].mean()
    return (out / 255.0).reshape(size, size, 1)

def load_session(session_dir):
    # cached: PNG decode + base crops go to one .npz, keyed by dir mtime
    cache = os.path.join(session_dir, f'.tfcache-{args.size}-c{args.cams}-v5.npz')
    newest = max((os.path.getmtime(os.path.join(session_dir, f))
                  for f in os.listdir(session_dir) if f.endswith('.agi')), default=0)
    if os.path.exists(cache) and os.path.getmtime(cache) >= newest:
        z = np.load(cache, allow_pickle=True)
        return (z['tx'], z['ty'], list(z['tgray']),
                [z[k] for k in ('tl', 'tr', 'tf', 'bl', 'br', 'bf')],
                z['laux'], z['ly'], z['lg'], z['lmeta'], z['lfid'])
    r = load_session_uncached(session_dir)
    tx, ty, tgray, limgs, laux, ly, lg, lmeta, lfid = r
    np.savez_compressed(cache, tx=tx, ty=ty, tgray=np.array(tgray),
                        tl=limgs[0], tr=limgs[1], tf=limgs[2],
                        bl=limgs[3], br=limgs[4], bf=limgs[5],
                        laux=laux, ly=ly, lg=lg, lmeta=lmeta, lfid=lfid)
    return r

def load_session_uncached(session_dir):
    from PIL import Image
    global args
    size = args.size
    target_x, target_y, target_gray = [], [], []
    # look: one sample per FRAME, both cameras fused (stereo)
    look = {k: [] for k in ('tl', 'tr', 'tf', 'bl', 'br', 'bf')}
    look_aux, look_y, look_g, look_meta, look_fid = [], [], [], [], []
    groups = {}
    frame = 0
    while True:
        agi = os.path.join(session_dir, f'{frame}.agi')
        if not os.path.exists(agi):
            break
        text = open(agi).read()
        center = read_pair(text, 'center')
        offset = read_pair(text, 'offset')
        cams = {}
        for cam in ('top', 'bot'):
            left  = read_pair(text, f'{cam}_left')
            right = read_pair(text, f'{cam}_right')
            scale = read_pair(text, f'{cam}_scale')
            if left[0] < 0.0 or right[0] < 0.0 or scale[0] < 0.0:
                continue
            png = os.path.join(session_dir, f'{frame}-{cam}.png')
            if not os.path.exists(png):
                continue
            gray = np.asarray(Image.open(png).convert('L'))
            full = crop_tensor(gray, 0.0, 0.0, 0.0)
            target_x.append(full)
            target_y.append([left[0], left[1], right[0], right[1], scale[0]])
            target_gray.append(gray)
            side = max(scale[0] / 3.0, 0.05)
            med  = ((left[0] + right[0]) * 0.5, (left[1] + right[1]) * 0.5)
            cams[cam] = dict(
                l=crop_tensor(gray, left[0],  left[1],  side),
                r=crop_tensor(gray, right[0], right[1], side),
                f=crop_tensor(gray, med[0], med[1], max(scale[0], 0.1)),
                aux=[med[0], med[1], scale[0],
                     left[0], left[1], right[0], right[1]],
                meta=[len(target_gray) - 1, left[0], left[1],
                      right[0], right[1], scale[0]])
        sel = ('top', 'bot')[:args.cams]
        if center[0] >= 0.0 and all(c in cams for c in sel):
            slots = {'top': ('tl', 'tr', 'tf'), 'bot': ('bl', 'br', 'bf')}
            auxv, metav = [], []
            for c in sel:
                kl, kr, kf = slots[c]
                look[kl].append(cams[c]['l'])
                look[kr].append(cams[c]['r'])
                look[kf].append(cams[c]['f'])
                auxv += cams[c]['aux']
                metav += cams[c]['meta']
            look_aux.append(auxv)
            look_meta.append(metav)
            look_fid.append(frame)
            # head focus + absolute gaze (dot = exact ground truth)
            look_y.append([center[0], center[1],
                           center[0] + offset[0], center[1] + offset[1]])
            # frames sharing a head circle form one group (no leakage
            # between train and eval)
            gk = (round(center[0], 5), round(center[1], 5))
            look_g.append(groups.setdefault(gk, len(groups)))
        frame += 1
    return (np.array(target_x), np.array(target_y, np.float32), target_gray,
            [np.array(look[k]) for k in ('tl', 'tr', 'tf', 'bl', 'br', 'bf')],
            np.array(look_aux, np.float32), np.array(look_y, np.float32),
            np.array(look_g), np.array(look_meta, np.float32), np.array(look_fid))

def aug_crop(gray, cx, cy, side):
    # like crop_tensor but out-of-frame samples are BLACK: zero-pad the
    # source so every window read stays in-bounds
    global args
    size = args.size
    h, w = gray.shape
    pad  = w
    gp   = np.pad(gray.astype(np.float32), pad)
    img_w   = float(w)
    side_px = side * img_w
    x0      = cx * img_w - side_px * 0.5 + pad
    y0      = cy * img_w - side_px * 0.5 + pad
    step    = side_px / size
    out = np.zeros((size, size), np.float32)
    for oy in range(size):
        ys, ye = int(y0 + oy * step), int(y0 + (oy + 1) * step)
        if ye <= ys: ye = ys + 1
        for ox in range(size):
            xs, xe = int(x0 + ox * step), int(x0 + (ox + 1) * step)
            if xe <= xs: xe = xs + 1
            out[oy, ox] = gp[ys:ye, xs:xe].mean()
    return (out / 255.0).reshape(size, size, 1)

def rot_crop(gray, cx, cy, side, deg):
    # rotated dest grid, coverage-averaged via step-matched subsampling;
    # black outside the frame (full-frame pad)
    global args
    size = args.size
    h, w = gray.shape
    pad  = w
    gp   = np.pad(gray.astype(np.float32), pad)
    sp   = side * w
    rad  = np.deg2rad(deg)
    cr, sr = np.cos(rad), np.sin(rad)
    ns   = int(min(max(sp / size, 1) + 2, 12))
    o    = (np.arange(ns) + 0.5) / ns
    u    = ((np.arange(size)[:, None] + o[None, :]).reshape(-1) / size) - 0.5
    UU, VV = np.meshgrid(u, u, indexing='xy')
    dx, dy = UU * sp, VV * sp
    sx = np.clip((cx * w + cr * dx - sr * dy + pad).astype(int), 0, w + 2 * pad - 1)
    sy = np.clip((cy * w + sr * dx + cr * dy + pad).astype(int), 0, h + 2 * pad - 1)
    vals = gp[sy, sx].reshape(size, ns, size, ns).mean(axis=(1, 3))
    return (vals / 255.0).reshape(size, size, 1)

def rotate_label(l, cx, cy, ws, deg):
    rad = np.deg2rad(deg)
    cr, sr = np.cos(rad), np.sin(rad)
    out = []
    for i in (0, 2):
        dx, dy = l[i] - cx, l[i + 1] - cy
        out += [( cr * dx + sr * dy) / ws + 0.5,
                (-sr * dx + cr * dy) / ws + 0.5]
    return [out[0], out[1], out[2], out[3], l[4] / ws]

def mirror_label(l):
    return [1.0 - l[2], l[3], 1.0 - l[0], l[1], l[4]]

def augment_target(grays, labels, idxs):
    # per train frame: original, mirror, 3 shifted/zoomed windows (half mirrored)
    xs, ys = [], []
    for i in idxs:
        g, lbl = grays[i], list(labels[i])
        full = crop_tensor(g, 0.0, 0.0, 0.0)
        xs.append(full); ys.append(lbl)
        if args.mirror:
            xs.append(full[:, ::-1, :].copy()); ys.append(mirror_label(lbl))
        for _ in range(16):
            ws  = 0.7 + np.random.rand() * 0.3
            lo, hi = ws * 0.5 - 0.15, 1.0 - ws * 0.5 + 0.15
            wcx = lo + np.random.rand() * (hi - lo)
            wcy = lo + np.random.rand() * (hi - lo)
            deg = (np.random.rand() - 0.5) * 30.0
            al  = rotate_label(lbl, wcx, wcy, ws, deg)
            if not all(0.02 <= v <= 0.98 for v in al[:4]):
                continue
            a = rot_crop(g, wcx, wcy, ws, deg)
            xs.append(a); ys.append(al)
            if args.mirror:
                xs.append(a[:, ::-1, :].copy()); ys.append(mirror_label(al))
    return np.array(xs), np.array(ys, np.float32)

def refine_window(lbl, jitter):
    mx, my = (lbl[0] + lbl[2]) * 0.5, (lbl[1] + lbl[3]) * 0.5
    s = lbl[4]
    # scale x0.5..1.66 (doubled variance), center +-0.15 frame
    ws = max(s * 1.25 * ((0.5 + np.random.rand() * 1.16) if jitter else 1.0), 0.15)
    cx = mx + ((np.random.rand() - 0.5) * 0.3 if jitter else 0.0)
    cy = my - ws * 0.1 + ((np.random.rand() - 0.5) * 0.3 if jitter else 0.0)
    deg = (np.random.rand() - 0.5) * 30.0 if jitter else 0.0
    al  = rotate_label(lbl, cx, cy, ws, deg)
    return cx, cy, ws, deg, al

def photo_variants(img, n=4):
    # brightness/contrast jitter around identity; labels unchanged
    outs = []
    for _ in range(n):
        c = 0.7 + np.random.rand() * 0.7      # contrast 0.7 .. 1.4
        b = (np.random.rand() - 0.5) * 0.3    # brightness -0.15 .. +0.15
        outs.append(np.clip((img - 0.5) * c + 0.5 + b, 0.0, 1.0))
    return outs

def augment_refine(grays, labels, idxs, jitter, per=32, enrich=None):
    # jitter = window position/scale/rotation variance (eval wants this too,
    # for live-like framing); enrich = mirror+photometric copies (train only)
    if enrich is None:
        enrich = jitter
    xs, ys = [], []
    for i in idxs:
        g, lbl = grays[i], list(labels[i])
        for _ in range(per):
            cx, cy, ws, deg, al = refine_window(lbl, jitter)
            if jitter and not all(0.02 <= v <= 0.98 for v in al[:4]):
                continue
            a = rot_crop(g, cx, cy, ws, deg)
            xs.append(a); ys.append(al)
            if enrich:
                for pv in photo_variants(a):
                    xs.append(pv); ys.append(al)
            if enrich and args.mirror:
                am, alm = a[:, ::-1, :].copy(), mirror_label(al)
                xs.append(am); ys.append(alm)
                for pv in photo_variants(am):
                    xs.append(pv); ys.append(alm)
    return np.array(xs), np.array(ys, np.float32)

def augment_look(grays, limgs, laux, ly, lmeta, idxs, n_scale=4, n_photo=3):
    # silver parity: 16 scale-grouped re-crops (ONE 0.9..1.1 factor shared
    # by all crops + aux scale) x (1 + 3 photometric) = 64x
    nc = args.cams
    xs = [[] for _ in range(3 * nc)]
    auxs, ys = [], []
    def emit_one(crops, auxv, y):
        for k in range(3 * nc):
            xs[k].append(crops[k])
        auxs.append(auxv); ys.append(y)
        for _ in range(n_photo):
            for k in range(3 * nc):
                c = 0.7 + np.random.rand() * 0.7
                b = (np.random.rand() - 0.5) * 0.3
                xs[k].append(np.clip((crops[k] - 0.5) * c + 0.5 + b, 0.0, 1.0))
            auxs.append(auxv); ys.append(y)
    # no mirror for look: flipped faces measurably hurt gaze (A/B'd)
    emit = emit_one
    for i in idxs:
        emit([limgs[k][i] for k in range(3 * nc)], laux[i], ly[i])
        for _ in range(n_scale - 1):
            s = 0.9 + np.random.rand() * 0.2
            d = 0.0   # no rotation: it would change the screen point
            crops, auxv = [], list(laux[i])
            for c in range(nc):
                tgi, lx, lyv, rx, ry, sc = lmeta[i][c * 6:(c + 1) * 6]
                g = grays[int(tgi)]
                sc2  = sc * s
                side = max(sc2 / 3.0, 0.05)
                med  = ((lx + rx) * 0.5, (lyv + ry) * 0.5)
                crops += [rot_crop(g, lx, lyv, side, d),
                          rot_crop(g, rx, ry, side, d),
                          rot_crop(g, med[0], med[1], max(sc2, 0.1), d)]
                auxv[c * 7 + 2] = sc2
            emit(crops, np.array(auxv, np.float32), ly[i])
    return ([np.array(x) for x in xs], np.array(auxs, np.float32),
            np.array(ys, np.float32))

def stamp_rows(imgs, auxarr):
    # row 0 of every crop IS the crop info: zeroed, first 7 px = that
    # camera's aux. stamped after all pixel augmentation.
    out = []
    for k, im in enumerate(imgs):
        im = np.array(im, np.float32, copy=True)
        im[:, 0, :, 0] = 0.0
        im[:, 0, :7, 0] = auxarr[:, (k // 3) * 7:(k // 3) * 7 + 7]
        out.append(im)
    return out

def build_target(tf):
    global args
    size = args.size
    x = tf.keras.Input((size, size, 1))
    h = tf.keras.layers.Conv2D(8, 3, padding='same', activation='relu')(x)
    h = tf.keras.layers.MaxPool2D(2)(h)
    if size >= 64:
        # deeper stage so the extra resolution becomes receptive field,
        # not memorizable noise
        h = tf.keras.layers.Conv2D(16, 3, padding='same', activation='relu')(h)
        h = tf.keras.layers.MaxPool2D(2)(h)
    h = tf.keras.layers.Flatten()(h)
    h = tf.keras.layers.Dense(64, activation='relu')(h)
    y = tf.keras.layers.Dense(5)(h)
    return tf.keras.Model(x, y)

def build_look(tf):
    global args
    size = args.size
    # 3 image branches per camera (top first, then bot when --cams 2)
    # named: tflite scrambles input order, the app matches by name
    inames = ['inTL', 'inTR', 'inTF', 'inBL', 'inBR', 'inBF'][:3 * args.cams]
    imgs = [tf.keras.Input((size, size, 1), name=nm) for nm in inames]
    aux  = tf.keras.Input((7 * args.cams,), name='aux')
    if args.export:
        # exact mirror of silver build_look so the .bin drops into the
        # silver testbed: conv16x3x3 per branch, pool, flatten,
        # dense64, +aux, dense64, dense4
        convs = [tf.keras.layers.Conv2D(16, 3, padding='same', activation='relu',
                                        name=f'conv{k}')(i)
                 for k, i in enumerate(imgs)]
        h = tf.keras.layers.Concatenate(axis=-1)(convs)
        h = tf.keras.layers.MaxPool2D(2)(h)
        h = tf.keras.layers.Flatten()(h)
        h = tf.keras.layers.Dense(64, activation='relu', name='dense1')(h)
        h = tf.keras.layers.Concatenate(axis=-1)([h, aux])
        h = tf.keras.layers.Dense(64, activation='relu', name='dense2')(h)
        y = tf.keras.layers.Dense(4, name='dense3')(h)
        return tf.keras.Model(imgs + [aux], y)
    if args.alternate:
        # old vision-train.py track net: stacked channels, depthwise
        # 5x5 x64/channel, pool, flatten, +aux, 64 -> 16 -> 4
        h = tf.keras.layers.Concatenate(axis=-1)(imgs)
        h = tf.keras.layers.DepthwiseConv2D(5, depth_multiplier=64,
                                            padding='same', activation='relu')(h)
        h = tf.keras.layers.MaxPool2D(2)(h)
        h = tf.keras.layers.Flatten()(h)
        h = tf.keras.layers.Concatenate(axis=-1)([h, aux])
        h = tf.keras.layers.Dense(64, activation='relu')(h)
        h = tf.keras.layers.Dense(16, activation='relu')(h)
        y = tf.keras.layers.Dense(4)(h)
        return tf.keras.Model(imgs + [aux], y)
    convs = [tf.keras.layers.Conv2D(16, 3, padding='same', activation='relu')(i)
             for i in imgs]
    h = tf.keras.layers.Concatenate(axis=-1)(convs)
    h = tf.keras.layers.MaxPool2D(2)(h)
    h = tf.keras.layers.Flatten()(h)
    h = tf.keras.layers.Dense(16, activation='relu')(h)
    a = aux
    if args.aux_dense:
        a = tf.keras.layers.Dense(args.aux_dense, activation='relu')(aux)
    if args.film:
        # aux MODULATES the image features (scale+shift per feature):
        # screen point = f(iris) conditioned on head pose — the coupling
        # is multiplicative, so the optimizer cannot bypass the aux
        g = tf.keras.layers.Dense(16, activation='tanh')(a)
        b = tf.keras.layers.Dense(16)(a)
        h = tf.keras.layers.Multiply()([h, tf.keras.layers.Lambda(lambda t: 1.0 + t)(g)])
        h = tf.keras.layers.Add()([h, b])
    h = tf.keras.layers.Concatenate(axis=-1)([h, a])
    h = tf.keras.layers.Dense(128, activation='relu')(h)
    y = tf.keras.layers.Dense(4)(h)
    return tf.keras.Model(imgs + [aux], y)

def silver_layer_names():
    return [f'conv{k}' for k in range(3 * args.cams)] + ['dense1', 'dense2', 'dense3']

def export_bin(tf, model, out_path, quiet=False):
    # silver save_weights order: per op (weights then bias), ops order =
    # convs (branch order) then dense1, dense2, dense3.  silver conv
    # layout (ic,kh,kw)xoc == TF (kh,kw,ic,oc) transposed
    buf = []
    for nm in silver_layer_names():
        k, b = model.get_layer(nm).get_weights()
        if k.ndim == 4:
            k = np.transpose(k, (2, 0, 1, 3)).reshape(-1, k.shape[-1])
        buf.append(k.astype(np.float32).ravel())
        buf.append(b.astype(np.float32).ravel())
    open(out_path, 'wb').write(np.concatenate(buf).tobytes())
    if not quiet:
        print(f'exported {sum(len(x) for x in buf)} floats -> {out_path}')

def export_tflite(tf, model, name):
    p = args.export_tflite
    if not p.endswith('.tflite'):
        p = os.path.join(p, f'{name}.tflite')
    conv = tf.lite.TFLiteConverter.from_keras_model(model)
    open(p, 'wb').write(conv.convert())
    print(f'exported tflite -> {p}')

def pad(txt, w=13):
    return str(txt)[:w - 1].ljust(w)

def run(tf, model, inputs, labels, args, out_labels, groups=None, presplit=None):
    size = args.size
    if presplit is not None:
        x_tr, y_tr, x_ev, y_ev = presplit
        print(f'split: {len(y_tr)} train (augmented) / {len(y_ev)} eval')
        return fit_loop(tf, model, x_tr, y_tr, x_ev, y_ev, args, out_labels)
    n = len(labels)
    if groups is not None:
        # hold out whole head-circle groups — eval never shares a circle
        gids = np.unique(groups)
        np.random.shuffle(gids)
        ev_g = set(gids[:max(1, len(gids) // 10)])
        ev   = np.array([i for i in range(n) if groups[i] in ev_g])
        tr   = np.array([i for i in range(n) if groups[i] not in ev_g])
    else:
        order  = np.random.permutation(n)
        n_eval = n // 10
        tr, ev = order[:n - n_eval], order[n - n_eval:]
    print(f'split: {len(tr)} train / {len(ev)} eval'
          + (f' ({len(ev_g)} of {len(gids)} circles held out)' if groups is not None else ''))
    x_tr = [x[tr] for x in inputs]
    x_ev = [x[ev] for x in inputs]
    y_tr, y_ev = labels[tr], labels[ev]
    return fit_loop(tf, model, x_tr, y_tr, x_ev, y_ev, args, out_labels)

def fit_loop(tf, model, x_tr, y_tr, x_ev, y_ev, args, out_labels):
    opt = (tf.keras.optimizers.Adam(args.lr) if args.optimizer == 'adam'
           else tf.keras.optimizers.SGD(args.lr))
    model.compile(opt, 'mse')
    # --preview proof: the dumped pngs ARE the vectors entering fit()
    if args.preview and len(x_tr) > 1:
        from PIL import Image as PImage
        pd = os.path.join(f'/src/hyperspace-sessions/{args.session}', 'preview-tf')
        ok = bad = 0
        for j in range(0, len(y_tr), 4):
            p = os.path.join(pd, f'look_{j:05d}.png')
            if not os.path.exists(p):
                continue
            saved = np.asarray(PImage.open(p))
            row = (np.hstack([x[j][:, :, 0] for x in x_tr[:-1]]) * 255).astype(np.uint8)
            if saved.shape == row.shape and np.array_equal(saved, row):
                ok += 1
            else:
                bad += 1
        print(f'preview vs training vectors: {ok} identical, {bad} MISMATCHED')
    if args.validate:
        # raw values entering fit(): global stats + first samples verbatim
        names2 = ([f'img{k}' for k in range(len(x_tr) - 1)] + ['aux']
                  if len(x_tr) > 1 else ['img0'])
        print('validate: raw training tensors')
        for nm, x in zip(names2, x_tr):
            x = np.asarray(x)
            print(f'  {nm}: shape {x.shape} dtype {x.dtype} '
                  f'min {x.min():.6f} max {x.max():.6f} mean {x.mean():.6f} '
                  f'nan {int(np.isnan(x).sum())}')
        yv = np.asarray(y_tr)
        print(f'  label: shape {yv.shape} min {yv.min():.6f} max {yv.max():.6f} '
              f'nan {int(np.isnan(yv).sum())}')
        # one sample per SOURCE FRAME (16 augment copies per frame sit
        # consecutively; consecutive rows sharing aux is by design)
        step = 16
        fids = getattr(args, '_val_fids', None)
        for f2 in range(min(8, len(y_tr) // step)):
            j = f2 * step
            if len(x_tr) > 1:
                tag = f'{fids[f2]}.agi' if fids is not None else f'frame {f2}'
                print(f'  {tag} (sample {j}): aux '
                      f'{np.array2string(np.asarray(x_tr[-1][j]), precision=4)}'
                      f'  label {np.array2string(yv[j], precision=4)}')
    print(pad('epoch', 10) + pad('train') + pad('eval') +
          ''.join(pad(c) for c in out_labels))
    # checkpoint like silver: keep + write best mean |err| epoch
    layer_set  = [l.name for l in model.layers]
    exportable = args.export and all(nm in layer_set for nm in silver_layer_names())
    best, best_w, best_ep = None, None, 0
    for epoch in range(args.epochs):
        hist = model.fit([x for x in x_tr], y_tr, batch_size=args.batch,
                         shuffle=True, epochs=1, verbose=0)
        p    = model.predict([x for x in x_ev], verbose=0)
        eval_loss = float(np.mean(np.mean((p - y_ev) ** 2, axis=1)))
        errs      = np.mean(np.abs(p - y_ev), axis=0)
        print(pad(f'{epoch + 1}/{args.epochs}', 10) +
              pad(f'{hist.history["loss"][0]:.6g}') +
              pad(f'{eval_loss:.6g}') +
              ''.join(pad(f'{e:.6g}') for e in errs))
        m = float(np.mean(errs))
        if best is None or m < best:
            best, best_ep = m, epoch + 1
            best_w = [w.copy() for w in model.get_weights()]
            if exportable:
                export_bin(tf, model, args.export, quiet=True)
    if best_w is not None:
        model.set_weights(best_w)
        print(f'best epoch {best_ep} (avg err {best:.6g})'
              + (f' -> {args.export}' if exportable else ''))
    print('training complete')

def main():
    args = parse_args()
    if args.seed:
        np.random.seed(args.seed)
    # port.cc chatter ignores TF_CPP_MIN_LOG_LEVEL; mute stderr on import
    err, devnull = os.dup(2), os.open(os.devnull, os.O_WRONLY)
    os.dup2(devnull, 2)
    try:
        import tensorflow as tf
    finally:
        os.dup2(err, 2)
        os.close(err)
        os.close(devnull)
    tf.get_logger().setLevel('ERROR')
    session_dir = f'/src/hyperspace-sessions/{args.session}'
    tx, ty, tgray, limgs, laux, ly, lgroups, lmeta, lfid = load_session(session_dir)
    n_te, n_le = len(ty) // 10, len(ly) // 10
    print(f'train: {len(ty) - n_te} target (+{n_te} eval), '
          f'{len(ly) - n_le} look (+{n_le} eval)')
    if len(ty) and args.model in ('all', 'target'):
        order = np.random.permutation(len(ty))
        ev_i, tr_i = order[:n_te], order[n_te:]
        ax, ay = augment_target(tgray, ty, tr_i)
        print(f'target augmented: {len(ay)} train / {len(ev_i)} eval')
        tm = build_target(tf)
        run(tf, tm, None, None, args,
            ['left.x', 'left.y', 'right.x', 'right.y', 'scale'],
            presplit=([ax], ay, [tx[ev_i]], ty[ev_i]))
        if args.export_tflite:
            export_tflite(tf, tm, 'target')
    if len(ty) and args.model in ('all', 'refine'):
        order = np.random.permutation(len(ty))
        ev_i, tr_i = order[:n_te], order[n_te:]
        rx_, ry_ = augment_refine(tgray, ty, tr_i, jitter=True)
        ex_, ey_ = augment_refine(tgray, ty, ev_i, jitter=False, per=1, enrich=False)
        print(f'refine: {len(ry_)} train / {len(ey_)} eval')
        rm = build_target(tf)
        run(tf, rm, None, None, args,
            ['left.x', 'left.y', 'right.x', 'right.y', 'scale'],
            presplit=([rx_], ry_, [ex_], ey_))
        if args.export_tflite:
            export_tflite(tf, rm, 'refine')
    want_look = args.model in ('all', 'look')
    experts = []
    if args.model == 'all':
        experts = [(x, y) for x in range(3) for y in range(2)]
    elif args.model.startswith('look_'):
        experts = [(int(args.model[5]), int(args.model[7]))]
    if len(ly) and (want_look or experts):
        # hold out whole head-circle groups; eval stays canonical/raw
        gids = np.unique(lgroups)
        np.random.shuffle(gids)
        ev_g = set(gids[:max(1, len(gids) // 10)])
        ev = np.array([i for i in range(len(ly)) if lgroups[i] in ev_g])
        tr = np.array([i for i in range(len(ly)) if lgroups[i] not in ev_g])
        ax, aaux, ay = augment_look(tgray, limgs, laux, ly, lmeta, tr)
        ax = stamp_rows(ax, aaux)
        print(f'look augmented: {len(ay)} train / {len(ev)} eval '
              f'({len(ev_g)} of {len(gids)} circles held out)')
        if args.preview:
            from PIL import Image as PImage
            pd = os.path.join(session_dir, 'preview-tf')
            os.makedirs(pd, exist_ok=True)
            # stride 4 lands on base + scale variants, skips photo dupes
            for j in range(0, len(ay), 4):
                row = np.hstack([ax[k][j][:, :, 0] for k in range(3 * args.cams)])
                PImage.fromarray((row * 255).astype(np.uint8)).save(
                    os.path.join(pd, f'look_{j:05d}.png'))
            # per canonical sample: full frame + the crop rects used
            # rects are NEVER shrunk to fit: frame sits on a black
            # margin so off-frame windows show at true position/size
            def draw_rect(img, cx, cy, side, iw, pad):
                h2, w2 = img.shape
                x0 = int(np.clip((cx - side / 2) * iw + pad, 0, w2 - 1))
                x1 = int(np.clip((cx + side / 2) * iw + pad, 0, w2 - 1))
                y0 = int(np.clip((cy - side / 2) * iw + pad, 0, h2 - 1))
                y1 = int(np.clip((cy + side / 2) * iw + pad, 0, h2 - 1))
                img[y0:y1 + 1, x0] = img[y0:y1 + 1, x1] = 255
                img[y0, x0:x1 + 1] = img[y1, x0:x1 + 1] = 255
            block = 4 * (1 + 3)   # n_scale x (1 + n_photo)
            for k2, i in enumerate(tr):
                j = k2 * block
                if j >= len(ay): break
                for c in range(args.cams):
                    tgi, lx, lyv, rx, ry, sc = lmeta[i][c * 6:(c + 1) * 6]
                    src = tgray[int(tgi)].astype(np.uint8)
                    iw = src.shape[1]
                    pad = iw // 3
                    fr = np.zeros((src.shape[0] + 2 * pad, iw + 2 * pad), np.uint8)
                    fr[pad:pad + src.shape[0], pad:pad + iw] = src
                    es = max(sc / 3.0, 0.05)
                    draw_rect(fr, lx, lyv, es, iw, pad)
                    draw_rect(fr, rx, ry, es, iw, pad)
                    draw_rect(fr, (lx + rx) / 2, (lyv + ry) / 2, max(sc, 0.1), iw, pad)
                    sfx = '' if args.cams == 1 else f'_c{c}'
                    PImage.fromarray(fr).save(
                        os.path.join(pd, f'look_{j:05d}_frame{sfx}.png'))
            print(f'preview: {(len(ay) + 3) // 4} crop pngs + {len(tr)} frame pngs -> {pd}')
        if args.zero_aux:
            aaux = np.zeros_like(aaux)
            laux = np.zeros_like(laux)
        if want_look:
            args._val_fids = lfid[tr]
            lm = build_look(tf)
            sev = stamp_rows([x[ev] for x in limgs[:3 * args.cams]], laux[ev])
            run(tf, lm, None, None, args,
                ['head.x', 'head.y', 'gaze.x', 'gaze.y'],
                presplit=(ax + [aaux], ay, sev + [laux[ev]], ly[ev]))
            if args.export:
                export_bin(tf, lm, args.export)
            if args.export_tflite:
                export_tflite(tf, lm, 'look')
        # regional experts on overlapping bands (silver parity):
        # x: [0-.5] [.25-.75] [.5-1]; y: [0-.75] [.25-1]
        for bx, by in experts:
            x0, y0 = bx * 0.25, by * 0.25
            x1, y1 = x0 + 0.5, y0 + 0.75
            # sparse-data compensation: re-augment ONLY this band's
            # frames at 64x depth (16x the shared pass)
            bly = ly[tr]
            in_band = ((bly[:, 2] >= x0) & (bly[:, 2] <= x1) &
                       (bly[:, 3] >= y0) & (bly[:, 3] <= y1))
            exa, exaux, exy = augment_look(tgray, limgs, laux, ly, lmeta,
                                           tr[in_band], n_scale=64)
            # mirror twins can flip out of band — filter the result
            m_tr = ((exy[:, 2] >= x0) & (exy[:, 2] <= x1) &
                    (exy[:, 3] >= y0) & (exy[:, 3] <= y1))
            ty2 = exy[m_tr].copy()
            ty2[:, 2] = (ty2[:, 2] - x0) / (x1 - x0)
            ty2[:, 3] = (ty2[:, 3] - y0) / (y1 - y0)
            ely = ly[ev]
            m_ev = ((ely[:, 2] >= x0) & (ely[:, 2] <= x1) &
                    (ely[:, 3] >= y0) & (ely[:, 3] <= y1))
            ey2 = ely[m_ev].copy()
            ey2[:, 2] = (ey2[:, 2] - x0) / (x1 - x0)
            ey2[:, 3] = (ey2[:, 3] - y0) / (y1 - y0)
            print(f'look_{bx}_{by}: {int(m_tr.sum())} train / {int(m_ev.sum())} eval')
            if not m_tr.any() or not m_ev.any():
                continue
            args._val_fids = lfid[tr][in_band]
            em = build_look(tf)
            sxe = stamp_rows([x[m_tr] for x in exa], exaux[m_tr])
            see = stamp_rows([x[ev][m_ev] for x in limgs[:3 * args.cams]], laux[ev][m_ev])
            run(tf, em, None, None, args,
                ['head.x', 'head.y', 'gaze.x', 'gaze.y'],
                presplit=(sxe + [exaux[m_tr]], ty2, see + [laux[ev][m_ev]], ey2))
            if args.export:
                export_bin(tf, em, args.export)
            if args.export_tflite:
                export_tflite(tf, em, f'look_{bx}_{by}')

if __name__ == '__main__':
    main()
