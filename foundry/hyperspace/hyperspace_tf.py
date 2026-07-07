#!/usr/bin/env python3
# TensorFlow reference for hyperspace --train: same nets, data, labels, args.
import argparse, os, re, sys
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
    from PIL import Image
    global args
    size = args.size
    target_x, target_y = [], []
    # look: one sample per FRAME, both cameras fused (stereo)
    look = {k: [] for k in ('tl', 'tr', 'tf', 'bl', 'br', 'bf')}
    look_aux, look_y, look_g = [], [], []
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
            side = max(scale[0] * 0.5, 0.05)
            med  = ((left[0] + right[0]) * 0.5, (left[1] + right[1]) * 0.5)
            cams[cam] = dict(
                l=crop_tensor(gray, left[0],  left[1],  side),
                r=crop_tensor(gray, right[0], right[1], side),
                f=crop_tensor(gray, med[0], med[1], 0.3),
                aux=[med[0], med[1], scale[0],
                     left[0], left[1], right[0], right[1]])
        if center[0] >= 0.0 and 'top' in cams and 'bot' in cams:
            look['tl'].append(cams['top']['l'])
            look['tr'].append(cams['top']['r'])
            look['tf'].append(cams['top']['f'])
            look['bl'].append(cams['bot']['l'])
            look['br'].append(cams['bot']['r'])
            look['bf'].append(cams['bot']['f'])
            look_aux.append(cams['top']['aux'] + cams['bot']['aux'])
            # head focus + absolute gaze (dot = exact ground truth)
            look_y.append([center[0], center[1],
                           center[0] + offset[0], center[1] + offset[1]])
            # frames sharing a head circle form one group (no leakage
            # between train and eval)
            gk = (round(center[0], 5), round(center[1], 5))
            look_g.append(groups.setdefault(gk, len(groups)))
        frame += 1
    return (np.array(target_x), np.array(target_y, np.float32),
            [np.array(look[k]) for k in ('tl', 'tr', 'tf', 'bl', 'br', 'bf')],
            np.array(look_aux, np.float32), np.array(look_y, np.float32),
            np.array(look_g))

def build_target(tf):
    global args
    size = args.size
    x = tf.keras.Input((size, size, 1))
    h = tf.keras.layers.Conv2D(8, 3, padding='same', activation='relu')(x)
    h = tf.keras.layers.MaxPool2D(2)(h)
    h = tf.keras.layers.Flatten()(h)
    h = tf.keras.layers.Dense(64, activation='relu')(h)
    y = tf.keras.layers.Dense(5)(h)
    return tf.keras.Model(x, y)

def build_look(tf):
    global args
    size = args.size
    # 6 image branches: top L/R/face + bot L/R/face, stereo-fused
    imgs = [tf.keras.Input((size, size, 1)) for _ in range(6)]
    aux  = tf.keras.Input((14,))
    convs = [tf.keras.layers.Conv2D(16, 3, padding='same', activation='relu')(i)
             for i in imgs]
    h = tf.keras.layers.Concatenate(axis=-1)(convs)
    h = tf.keras.layers.MaxPool2D(2)(h)
    h = tf.keras.layers.Flatten()(h)
    h = tf.keras.layers.Dense(16, activation='relu')(h)
    h = tf.keras.layers.Concatenate(axis=-1)([h, aux])
    h = tf.keras.layers.Dense(128, activation='relu')(h)
    y = tf.keras.layers.Dense(4)(h)
    return tf.keras.Model(imgs + [aux], y)

def pad(txt, w=13):
    return str(txt)[:w - 1].ljust(w)

def run(tf, model, inputs, labels, args, out_labels, groups=None):
    size = args.size
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
    opt = (tf.keras.optimizers.Adam(args.lr) if args.optimizer == 'adam'
           else tf.keras.optimizers.SGD(args.lr))
    model.compile(opt, 'mse')
    print(pad('epoch', 10) + pad('train') + pad('eval') +
          ''.join(pad(c) for c in out_labels))
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
    print('training complete')

def main():
    args = parse_args()
    import tensorflow as tf
    tf.get_logger().setLevel('ERROR')
    session_dir = f'/src/hyperspace-sessions/{args.session}'
    tx, ty, limgs, laux, ly, lgroups = load_session(session_dir)
    n_te, n_le = len(ty) // 10, len(ly) // 10
    print(f'train: {len(ty) - n_te} target (+{n_te} eval), '
          f'{len(ly) - n_le} look (+{n_le} eval)')
    #if len(ty):
    #    run(tf, build_target(tf), [tx], ty, args,
    #        ['left.x', 'left.y', 'right.x', 'right.y', 'scale'])
    if len(ly):
        run(tf, build_look(tf), limgs + [laux], ly, args,
            ['head.x', 'head.y', 'gaze.x', 'gaze.y'], groups=lgroups)

if __name__ == '__main__':
    main()
