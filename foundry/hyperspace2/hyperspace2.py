#!/usr/bin/env python3
# minimal look trainer for hyperspace2 synthetic sessions. samples are
# {i}.agi + {i}-face.png; eye/face crops are cut at the EXACT label plots
# (no detection, no augmentation — the eye crops are never jittered).
#
# label conventions (zero-centered, -0.5 left/top .. +0.5 right/bottom):
#   look / head        screen plots
#   face_left/right    resting eye-socket centers in the face image
#   face_*_oc          outer eye corners in the face image
#   face_scale         inter-eye distance, image-width fraction
# pixel position in the saved image = (plot + 0.5) * dims
import argparse, os, re, hashlib, glob, ctypes
import numpy as np
os.environ.setdefault('TF_CPP_MIN_LOG_LEVEL', '2')
# preload the pip nvidia wheels: TF dlopens by soname, which only resolves
# if the libs are already in the process image (no LD_LIBRARY_PATH needed)
for _so in sorted(glob.glob(os.path.expanduser(
        '~/.local/lib/python3.12/site-packages/nvidia/*/lib/lib*.so.*'))):
    try: ctypes.CDLL(_so)
    except OSError: pass
import tensorflow as tf
from tensorflow.keras import layers


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument('--session',  default='default')
    p.add_argument('--epochs',   type=int,   default=30)
    p.add_argument('--lr',       type=float, default=0.0001)
    p.add_argument('--batch',    type=int,   default=64)
    p.add_argument('--draw',     type=int,   default=20000)  # samples drawn per epoch
    p.add_argument('--size',     type=int,   default=32)
    p.add_argument('--seed',     type=int,   default=1234)
    p.add_argument('--wd',       type=float, default=0.0)
    p.add_argument('--eye_div',  type=float, default=2.12)   # eye crop side = scale/eye_div
    p.add_argument('--face_mul', type=float, default=1.75)   # face crop side = scale*face_mul
    # process: 'look' = gaze from stabilized crops; 'plot' = the stabilizer
    # (raw frame -> resting eye CENTERS + scale; centers, never pupils)
    p.add_argument('--process',  default='look', choices=['look', 'plot'])
    p.add_argument('--plot_size', type=int, default=128)     # stabilizer input side
    # train-time aux jitter: keeps the geometry signal, kills the
    # per-sample fingerprint (and matches the stabilizer's runtime error)
    p.add_argument('--aux_noise', type=float, default=0.0)
    # train-time sensor model: per-sample gain jitter + per-pixel gaussian
    # noise. renders are exact, so without this the net memorizes pixels
    p.add_argument('--px_noise', type=float, default=0.02)
    # optics blur on the GENERATED frames at load: gaussian radius drawn
    # per sample between HALF of this and this, as a fraction of the
    # frame side (0.11%..0.22% of the image). baked into the cache; 0 = off
    p.add_argument('--blur', type=float, default=0.0022)
    p.add_argument('--zero_aux', action='store_true')  # zero aux everywhere
    # weight of the pupil heat-map loss: eye channel 0 is supervised to a
    # Gaussian bump at the labeled pupil (its own radius as sigma) and
    # penalized for any response outside it. 0 disables
    p.add_argument('--pupil_w',  type=float, default=0.3)
    p.add_argument('--stats',    action='store_true')
    p.add_argument('--preview',  action='store_true')
    return p.parse_args()

args = parse_args()


def read_pair(text, key):
    m = re.search(rf'{re.escape(key)}:\s*(-?[\d.]+)(?:[ \t]+(-?[\d.]+))?', text)
    if not m:
        return (-999.0, -999.0)
    return (float(m.group(1)), float(m.group(2)) if m.group(2) else 0.0)


def crop_gray(gray, cx, cy, side, out):
    # square crop centered at (cx, cy); out-of-frame padded, never clamped
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
    arr = np.asarray(c.resize((out, out), Image.BOX), np.float32)[..., None] / 255.0
    return arr, x0, y0, s


def read3(text, key):
    m = re.search(rf'{re.escape(key)}:\s*(-?[\d.eE+-]+)\s+(-?[\d.eE+-]+)\s+(-?[\d.eE+-]+)', text)
    return [float(m.group(i)) for i in (1, 2, 3)] if m else [0.0, 0.0, 0.0]


def load_one(job):
    # one job = one .agi; returns one training sample PER CAMERA view
    session_dir, fid = job
    text = open(os.path.join(session_dir, f'{fid}.agi')).read()
    cams = int(read_pair(text, 'cameras')[0])
    if cams <= 0:
        # legacy single-camera .agi: unsuffixed fields, {i}-face.png
        return [s for s in [load_cam(session_dir, fid, text, '', '')] if s]
    out = []
    for c in range(cams):
        s = load_cam(session_dir, fid, text, str(c), str(c))
        if s:
            out.append(s)
    return out


def load_cam(session_dir, fid, text, sfx, img_sfx):
    from PIL import Image, ImageDraw
    head = read_pair(text, 'head')
    look = read_pair(text, 'look')
    el   = read_pair(text, f'face_left{sfx}')
    er   = read_pair(text, f'face_right{sfx}')
    ocl  = read_pair(text, f'face_left_oc{sfx}')
    ocr  = read_pair(text, f'face_right_oc{sfx}')
    head_rotation = read3(text, 'head_rot')
    face_offset   = read3(text, 'face_off')
    sc   = read_pair(text, f'face_scale{sfx}')[0]
    # pupil centers (frame coords) + radius as inter-eye fraction; older
    # sessions lack them — sigma 0 gates the pupil map loss off
    pl   = read_pair(text, f'pupil_left{sfx}')
    pr   = read_pair(text, f'pupil_right{sfx}')
    prad = read_pair(text, 'pupil_rad')[0]
    # clean: 1 marks a non-augmented pose group — the eval set
    clean = 1.0 if re.search(r'clean:\s*1', text) else 0.0
    if look[0] < -900 or el[0] < -900 or sc <= 0:
        return None
    img = Image.open(os.path.join(session_dir, f'{fid}-face{img_sfx}.png')).convert('L')
    if args.blur > 0:
        # per-sample optics defocus on the WHOLE generated frame, before
        # any crop: deterministic per (sample, camera) so cache rebuilds
        # reproduce; radius in fractions of the frame side
        from PIL import ImageFilter
        brng = np.random.RandomState(fid * 31 + (int(img_sfx) if img_sfx else 0))
        brad = (args.blur / 2.0 + brng.rand() * (args.blur / 2.0)) * img.width
        img = img.filter(ImageFilter.GaussianBlur(brad))
    gray = np.asarray(img, np.float32) / 255.0
    H, W = gray.shape
    # plots -> pixels: p = (plot + 0.5) * dims
    lx, lyv = (el[0] + 0.5) * W, (el[1] + 0.5) * H
    rx, ryv = (er[0] + 0.5) * W, (er[1] + 0.5) * H
    side = sc * W / args.eye_div
    lc, lx0, ly0, ls = crop_gray(gray, lx, lyv, side, args.size)
    rc, rx0, ry0, rs = crop_gray(gray, rx, ryv, side, args.size)
    fx, fy = (lx + rx) / 2, (lyv + ryv) / 2
    fc, fx0, fy0, fs = crop_gray(gray, fx, fy, sc * W * args.face_mul, args.size)
    # whole frame, box-reduced: the stabilizer's input
    wf = np.asarray(img.resize((args.plot_size, args.plot_size), Image.BOX),
                    np.float32)[..., None] / 255.0
    # actual crop box: zero-centered center, side as image-width fraction
    def box(x0, y0, s):
        return [(x0 + s / 2) / W - 0.5, (y0 + s / 2) / H - 0.5, s / W]
    # head_rotation + face_offset = the origin-and-angle RAY: the head
    # label is a closed-form function of these six, so a net given them
    # must drive head error to ~0 or the trainer itself is broken
    aux = ([el[0], el[1], er[0], er[1],
            ocl[0], ocl[1], ocr[0], ocr[1], sc]
           + box(lx0, ly0, ls) + box(rx0, ry0, rs) + box(fx0, fy0, fs)
           + head_rotation + face_offset)
    y = [head[0], head[1], look[0], look[1]]
    # pupil labels in CROP fractions (0..1, the soft-argmax space) +
    # sigma = pupil radius as a crop-side fraction. invalid -> sigma 0
    def in_crop(p, x0, y0, s):
        return ((p[0] + 0.5) * W - x0) / s, ((p[1] + 0.5) * H - y0) / s
    plu, plv = in_crop(pl, lx0, ly0, ls)
    pru, prv = in_crop(pr, rx0, ry0, rs)
    sig = prad * sc * W / ls if (prad > 0 and pl[0] > -900) else 0.0
    if not (0.0 < plu < 1.0 and 0.0 < plv < 1.0 and
            0.0 < pru < 1.0 and 0.0 < prv < 1.0):
        sig = 0.0
    pup = [plu, plv, pru, prv, sig, clean]
    # ALWAYS write {i}-preview.png beside the sample: the annotated frame
    # is the permanent validation of every label the net eats (512px —
    # the full-res encode was the slow part of loading)
    pim = img.convert('RGB')
    dr = ImageDraw.Draw(pim)
    def cross(x, y2, col, r2=4):
        px, py = (x + 0.5) * W, (y2 + 0.5) * H
        dr.line([px - r2, py, px + r2, py], fill=col, width=2)
        dr.line([px, py - r2, px, py + r2], fill=col, width=2)
    cross(el[0], el[1], (0, 255, 0))
    cross(er[0], er[1], (0, 255, 0))
    if sig > 0:
        cross(pl[0], pl[1], (255, 0, 255))
        cross(pr[0], pr[1], (255, 0, 255))
    dr.rectangle([lx0, ly0, lx0 + ls, ly0 + ls], outline=(255, 64, 64), width=2)
    dr.rectangle([rx0, ry0, rx0 + rs, ry0 + rs], outline=(64, 128, 255), width=2)
    dr.rectangle([fx0, fy0, fx0 + fs, fy0 + fs], outline=(255, 255, 255), width=2)
    pim = pim.resize((512, 512), Image.BOX)
    dr = ImageDraw.Draw(pim)
    dr.text((6, 6), f'head {head[0]:+.3f} {head[1]:+.3f}  '
                    f'gaze {look[0]:+.3f} {look[1]:+.3f}  scale {sc:.3f}',
            fill=(0, 255, 0))
    pim.save(os.path.join(session_dir, f'{fid}-preview{img_sfx}.png'))
    # the FINISHED network inputs, side by side (left | right | face),
    # exactly as they go to the tensors — nearest-upscaled 4x so every
    # training pixel stays inspectable
    strip = np.concatenate([lc, rc, fc], axis=1)[:, :, 0]
    simg = Image.fromarray((strip * 255).astype(np.uint8))
    simg = simg.resize((simg.width * 4, simg.height * 4), Image.NEAREST)
    simg.save(os.path.join(session_dir, f'{fid}-inputs{img_sfx}.png'))
    return lc, rc, fc, wf, aux, y, fid, pup


def load_session(session_dir):
    ids = sorted(int(f[:-4]) for f in os.listdir(session_dir)
                 if f.endswith('.agi') and f[:-4].isdigit())
    if not ids:
        raise SystemExit(f'no .agi samples in {session_dir}')
    # folder mtime catches deletions too, not just newer samples
    newest = max(max(os.path.getmtime(os.path.join(session_dir, f'{i}.agi')) for i in ids),
                 os.path.getmtime(session_dir))
    cache = os.path.join(session_dir,
        f'.torchcache-look2-v7-b{args.blur}-e{args.eye_div}-f{args.face_mul}-s{args.size}-p{args.plot_size}.npz')
    if os.path.exists(cache) and os.path.getmtime(cache) >= newest:
        z = np.load(cache)
        return ([z[k] for k in ('tl', 'tr', 'tf', 'tw')], z['laux'], z['ly'],
                z['lfid'], z['lpup'])
    print(f'loading {len(ids)} samples ...')
    from concurrent.futures import ProcessPoolExecutor
    with ProcessPoolExecutor() as ex:
        rows = [s for group in ex.map(load_one, [(session_dir, i) for i in ids],
                                      chunksize=16) for s in group]
    tl   = [r2[0] for r2 in rows]
    tr   = [r2[1] for r2 in rows]
    tf_  = [r2[2] for r2 in rows]
    tw   = [r2[3] for r2 in rows]
    laux = [r2[4] for r2 in rows]
    ly   = [r2[5] for r2 in rows]
    lfid = [r2[6] for r2 in rows]
    lpup = [r2[7] for r2 in rows]
    r = ([np.array(x, np.float32) for x in (tl, tr, tf_, tw)],
         np.array(laux, np.float32), np.array(ly, np.float32),
         np.array(lfid), np.array(lpup, np.float32))
    np.savez_compressed(cache, tl=r[0][0], tr=r[0][1], tf=r[0][2], tw=r[0][3],
                        laux=r[1], ly=r[2], lfid=r[3], lpup=r[4])
    return r


NAUX = 24

def conv_bn(co, k=3, s=1):
    return tf.keras.Sequential([
        layers.Conv2D(co, k, strides=s, padding='same', use_bias=False),
        layers.BatchNormalization(), layers.ReLU()])


class Enc(tf.keras.Model):
    # crop encoder: BN conv trunk at full resolution -> k soft-argmax
    # sub-pixel (x,y) coordinates (a 1px iris shift moves them directly)
    # + 2x2-pooled appearance features. no Flatten->Dense on full-res
    # maps: that flat layer was the memorization surface; the 2x2 pool
    # keeps coarse layout and WHAT is seen, the coordinate path keeps
    # exact WHERE.
    def __init__(self, side, k=8, w=64):
        super().__init__()
        self.k = k
        self.trunk = tf.keras.Sequential([
            conv_bn(32, 5), conv_bn(w), conv_bn(w)])
        self.heat = layers.Conv2D(k, 1)
        self.temp = tf.Variable(8.0)
        self.lin = tf.constant(np.linspace(0.0, 1.0, side, dtype=np.float32))
        self.app = tf.keras.Sequential([
            conv_bn(w, s=2), conv_bn(2 * w, s=2), conv_bn(2 * w, s=2),
            layers.AveragePooling2D(pool_size=max(1, side // 16)),
            layers.Flatten()])
        self.fe = 2 * k + 2 * w * 4            # coords + pooled appearance

    def call(self, x, training=False, want_hm=False):
        t = self.trunk(x, training=training)   # full resolution, no pooling
        hm = self.heat(t)                      # B,S,S,k
        S = hm.shape[1]
        p = tf.nn.softmax(tf.reshape(hm, [-1, S * S, self.k]) * self.temp, axis=1)
        p = tf.reshape(p, [-1, S, S, self.k])
        xs = tf.einsum('bsk,s->bk', tf.reduce_sum(p, axis=1), self.lin)  # sub-pixel x
        ys = tf.einsum('bsk,s->bk', tf.reduce_sum(p, axis=2), self.lin)  # sub-pixel y
        out = tf.concat([xs, ys, self.app(t, training=training)], 1)     # B, self.fe
        return (out, hm) if want_hm else out


class LookNet(tf.keras.Model):
    # aux is REQUIRED geometry: where the crops sit in the frame and the
    # head rotation anchor — the crops alone cannot know either. one
    # shared filter bank reads both eyes; head from face + aux; gaze =
    # detached head anchor + delta from eyes + face + aux.
    def __init__(self):
        super().__init__()
        self.eye = Enc(args.size)
        self.face_enc = Enc(args.size)
        # aux gets its OWN unshared path: 24 clean floats drown under 528
        # conv features in a shared dense (proven: aux-only MLP beats the
        # fused net 3x on head). the image path is a zero-init correction
        self.head_aux = tf.keras.Sequential([
            layers.Dense(128, activation='relu'),
            layers.Dense(128, activation='relu'), layers.Dense(2)])
        self.head_img = tf.keras.Sequential([
            layers.Dense(128, activation='relu'),
            layers.Dense(2, kernel_initializer='zeros')])
        self.delta = tf.keras.Sequential([
            layers.Dense(128, activation='relu'), layers.Dense(2)])

    def pupil_map_loss(self, hm, u, v, sig):
        # heat channel 0 must BE the pupil: cross-entropy to a Gaussian
        # bump of the pupil's own radius, plus direct penalty on any
        # response mass outside 2.5 sigma — the "only learn from this
        # region" weight map. sig 0 (no label) gates a sample off.
        S = hm.shape[1]
        p = tf.nn.softmax(tf.reshape(hm[..., 0], [-1, S * S]) * self.eye.temp, 1)
        lin = tf.linspace(0.0, 1.0, S)
        gx = tf.reshape(tf.tile(lin[None, :], [S, 1]), [-1])   # x per cell
        gy = tf.reshape(tf.tile(lin[:, None], [1, S]), [-1])   # y per cell
        s2 = tf.maximum(sig, 1e-3)[:, None] ** 2
        d2 = ((gx[None, :] - u[:, None]) ** 2
            + (gy[None, :] - v[:, None]) ** 2) / (2.0 * s2)
        bump = tf.exp(-d2)
        bump = bump / (tf.reduce_sum(bump, 1, keepdims=True) + 1e-9)
        ce = -tf.reduce_sum(bump * tf.math.log(p + 1e-9), 1)
        outside = tf.reduce_sum(p * tf.cast(d2 > 3.125, tf.float32), 1)
        valid = tf.cast(sig > 0.0, tf.float32)
        return tf.reduce_sum(valid * (ce + outside)) / (tf.reduce_sum(valid) + 1e-6)

    def call(self, inputs, training=False):
        l, r, f, a, pl, pr = inputs
        fc = self.face_enc(f, training=training)
        el, hml = self.eye(l, training=training, want_hm=True)
        er, hmr = self.eye(r, training=training, want_hm=True)
        head = self.head_aux(a) + self.head_img(fc)
        # gaze = anchored head + delta, with head DETACHED in the sum:
        # otherwise the (pixel-limited) gaze loss backprops through the
        # sum and drags head off its closed-form aux solution
        hsg = tf.stop_gradient(head)
        d = self.delta(tf.concat([el, er, fc, a, hsg], 1))
        if training and args.pupil_w > 0:
            self.add_loss(args.pupil_w * (
                self.pupil_map_loss(hml, pl[:, 0], pl[:, 1], pl[:, 2])
              + self.pupil_map_loss(hmr, pr[:, 0], pr[:, 1], pr[:, 2])))
        return tf.concat([head, hsg + d], 1)


class PlotNet(tf.keras.Model):
    # the stabilizer: raw frame -> resting eye centers + scale. two of the
    # soft-argmax channels are supervised directly as the eye centers, so
    # position never squeezes through pooled features.
    def __init__(self):
        super().__init__()
        S = args.plot_size
        self.trunk = tf.keras.Sequential([
            layers.Conv2D(8, 5, padding='same', activation='relu'),
            layers.MaxPooling2D(2),
            layers.Conv2D(16, 3, padding='same', activation='relu'),
            layers.Conv2D(16, 3, padding='same', activation='relu')])
        self.heat = layers.Conv2D(2, 1)         # ch0 = left eye, ch1 = right
        self.temp = tf.Variable(8.0)
        self.lin = tf.constant(np.linspace(-0.5, 0.5, S // 2, dtype=np.float32))
        self.app = tf.keras.Sequential([
            layers.MaxPooling2D(2),
            layers.Conv2D(16, 3, padding='same', activation='relu'),
            layers.MaxPooling2D(2),
            layers.Conv2D(16, 3, padding='same', activation='relu'),
            layers.MaxPooling2D(2),
            layers.Flatten(),
            layers.Dense(32, activation='relu'),
            layers.Dense(1)])                   # scale

    def call(self, x, training=False):
        t = self.trunk(x, training=training)
        hm = self.heat(t)                       # B,S,S,2
        S = hm.shape[1]
        p = tf.nn.softmax(tf.reshape(hm, [-1, S * S, 2]) * self.temp, axis=1)
        p = tf.reshape(p, [-1, S, S, 2])
        xs = tf.einsum('bsk,s->bk', tf.reduce_sum(p, axis=1), self.lin)
        ys = tf.einsum('bsk,s->bk', tf.reduce_sum(p, axis=2), self.lin)
        sc = self.app(t, training=training)
        # [lx, ly, rx, ry, scale] in zero-centered frame fractions
        return tf.concat([xs[:, :1], ys[:, :1], xs[:, 1:], ys[:, 1:], sc], 1)


def fit(m, xtr, ytr, xev, yev, cols, metric, out_name):
    # keras graph path: compile + dataset + fit, nothing hand-rolled
    m.compile(optimizer=tf.keras.optimizers.Adam(learning_rate=args.lr), loss='mse')
    multi = isinstance(xtr, list)
    n = len(ytr)
    # an epoch is a fixed amount of WORK, not one pass: small sets repeat
    # up to `draw` samples so 30 epochs actually train
    draw = max(n, args.draw)
    steps = max(1, draw // args.batch)
    xs = tuple(xtr) if multi else xtr
    ds = tf.data.Dataset.from_tensor_slices((xs, ytr))
    ds = ds.shuffle(n).repeat().batch(args.batch)
    if multi and args.aux_noise > 0:
        def jitter(x, y):
            l, r, f, a = x[:4]
            return (l, r, f, a + tf.random.normal(tf.shape(a)) * args.aux_noise) + tuple(x[4:]), y
        ds = ds.map(jitter)
    if args.px_noise > 0:
        # one gain per sample across its crops (sensor exposure), then
        # independent per-pixel noise — train only, eval sees clean frames
        def sensor(x, y):
            def px(img, gain):
                return img * gain + tf.random.normal(tf.shape(img)) * args.px_noise
            if multi:
                l, r, f, a = x[:4]
                g = tf.random.uniform([tf.shape(l)[0], 1, 1, 1], 0.92, 1.08)
                return (px(l, g), px(r, g), px(f, g), a) + tuple(x[4:]), y
            g = tf.random.uniform([tf.shape(x)[0], 1, 1, 1], 0.92, 1.08)
            return px(x, g), y
        ds = ds.map(sensor)
    ds = ds.prefetch(tf.data.AUTOTUNE)
    xev_t = tuple(xev) if multi else xev
    print(f'epoch = {draw} draws ({steps} steps)')
    print('epoch      train        eval         ' + ''.join(f'{c:<13}' for c in cols))
    state = {'best': None, 'weights': None, 'ep': 0}

    class Row(tf.keras.callbacks.Callback):
        def on_epoch_end(self, epoch, logs=None):
            p = m.predict(xev_t, batch_size=len(yev), verbose=0)
            eval_loss = float(((p - yev) ** 2).mean())
            errs = np.abs(p - yev).mean(0)
            print(f'{epoch + 1:>2}/{args.epochs:<7}'
                  f'{logs["loss"]:<13.6g}{eval_loss:<13.6g}'
                  + ''.join(f'{e:<13.6g}' for e in errs))
            mval = float(metric(errs))
            if state['best'] is None or mval < state['best']:
                state['best'], state['ep'] = mval, epoch + 1
                state['weights'] = [w.copy() for w in m.get_weights()]

    m.fit(ds, epochs=args.epochs, steps_per_epoch=steps, verbose=0,
          callbacks=[Row()])
    if state['weights'] is not None:
        out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models')
        os.makedirs(out, exist_ok=True)
        m.set_weights(state['weights'])
        m.save_weights(os.path.join(out, out_name))
        print(f'best epoch {state["ep"]} (err {state["best"]:.6g}) -> {out}/{out_name}')


def train_plot(limgs, laux, ly, lfid):
    # targets: the annotated rest eye centers + scale, straight from .agi
    yt = np.concatenate([laux[:, 0:4], laux[:, 8:9]], 1).astype(np.float32)
    evm = np.array([int(hashlib.md5(f'{args.seed}_{f}'.encode()).hexdigest(), 16) % 10 == 0
                    for f in lfid])
    ev = np.where(evm)[0]
    tr = np.where(~evm)[0]
    print(f'plot: {len(tr)} train / {len(ev)} eval')
    m = PlotNet()
    fit(m, limgs[3][tr], yt[tr], limgs[3][ev], yt[ev],
        ['lx', 'ly', 'rx', 'ry', 'scale'],
        lambda errs: errs[:4].mean(), 'plot2.weights.h5')


def main():
    if args.seed:
        np.random.seed(args.seed)
        tf.random.set_seed(args.seed)
    # --session takes a comma list: sets concatenate, fids stay unique
    # per session so the eval holdout never pairs across sets
    names = [s.strip() for s in args.session.split(',') if s.strip()]
    sd = f'/src/hyperspace-sessions/{names[0]}'
    parts = [load_session(f'/src/hyperspace-sessions/{s}') for s in names]
    limgs = [np.concatenate([p[0][k] for p in parts]) for k in range(4)]
    laux = np.concatenate([p[1] for p in parts])
    ly   = np.concatenate([p[2] for p in parts])
    lfid = np.concatenate([p[3] + 1000000 * i for i, p in enumerate(parts)])
    lpup = np.concatenate([p[4] for p in parts])
    # per-eye label tensors for the pupil map loss: [u, v, sigma]
    plt_ = lpup[:, [0, 1, 4]].astype(np.float32)
    prt_ = lpup[:, [2, 3, 4]].astype(np.float32)
    nvalid = int((lpup[:, 4] > 0).sum())
    print(f'pupil labels: {nvalid}/{len(lpup)} valid')
    if args.process == 'plot':
        train_plot(limgs, laux, ly, lfid)
        return
    if args.stats:
        n = len(ly)
        print(f'== dataset: {args.session} ({n} samples) ==')
        for name, pts in (('gaze', ly[:, 2:4]), ('head', ly[:, :2])):
            print(f'  {name}: x {pts[:,0].min():+.3f}..{pts[:,0].max():+.3f}'
                  f'  y {pts[:,1].min():+.3f}..{pts[:,1].max():+.3f}')
        print(f'  scale: mean {laux[:,8].mean():.3f} span {laux[:,8].min():.3f}..{laux[:,8].max():.3f}')
        return
    if args.preview:
        # previews are ALWAYS written as {i}-preview.png beside each sample
        # during cache build; this flag just forces that rebuild
        for s in names:
            d = f'/src/hyperspace-sessions/{s}'
            for f in os.listdir(d):
                if f.startswith('.torchcache-'):
                    os.remove(os.path.join(d, f))
            load_session(d)
        print('previews rebuilt beside samples ({i}-preview.png)')
        return
    # eval = the CLEAN (non-augmented) pose groups; augmented frames must
    # never grade themselves. hash holdout only for old sessions without
    # clean flags
    if lpup[:, 5].max() > 0:
        evm = lpup[:, 5] > 0.5
        print(f'eval on clean frames: {int(evm.sum())}')
    else:
        evm = np.array([int(hashlib.md5(f'{args.seed}_{f}'.encode()).hexdigest(), 16) % 10 == 0
                        for f in lfid])
        print('no clean frames in session — hash holdout eval')
    ev = np.where(evm)[0]
    tr = np.where(~evm)[0]
    print(f'look: {len(tr)} train / {len(ev)} eval')

    atr, aev = laux[tr], laux[ev]
    if args.zero_aux:
        atr = np.zeros_like(atr)
        aev = np.zeros_like(aev)
    m = LookNet()
    fit(m, [limgs[0][tr], limgs[1][tr], limgs[2][tr], atr, plt_[tr], prt_[tr]], ly[tr],
        (limgs[0][ev], limgs[1][ev], limgs[2][ev], aev, plt_[ev], prt_[ev]), ly[ev],
        ['head.x', 'head.y', 'gaze.x', 'gaze.y'],
        lambda errs: errs[2:].mean(), 'look2.weights.h5')


if __name__ == '__main__':
    main()
