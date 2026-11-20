#!/usr/bin/env python3
# orbiter's windows and instances, driven as the user does: a hidden
# orbiter in a box (its own home), its socket, its STATE dump
import os, socket, subprocess, sys, tempfile, threading, time

ROOT = '/src/silver'
BOX = ROOT + '/platform/native/build/fp6-box'
ORB = ROOT + '/platform/native/build/orbiter'

class Box:
    def __init__(s, extra='', env=''):
        s.dir = tempfile.mkdtemp(prefix='orbtest-')
        for d in ('tmp', 'rt', 'home'):
            os.makedirs(s.dir + '/' + d)
        s.sock = s.dir + '/rt/trinity-orbiter.sock'
        s.log = s.dir + '/tmp/silver-orbiter.log'
        inner = s.dir + '/inner.sh'
        with open(inner, 'w') as f:
            f.write('#!/bin/sh\n'
                    f'mount --bind {s.dir}/tmp {ROOT}/platform/native/tmp'
                    ' || exit 1\n'
                    f'exec {BOX} --map-user=1000 --map-group=1000 -U '
                    f'env HOME={s.dir}/home XDG_RUNTIME_DIR={s.dir}/rt '
                    f'SILVER_HEADLESS=1 {env} {ORB} --hidden true '
                    f'--width 1280 --height 800 {extra}\n')
        os.chmod(inner, 0o755)
        s.proc = subprocess.Popen(
            ['timeout', '900', BOX, '-r', '-m', inner],
            cwd=ROOT, stdout=open(s.dir + '/orb.out', 'w'),
            stderr=subprocess.STDOUT, start_new_session=True)
        for _ in range(120):
            if os.path.exists(s.sock): break
            time.sleep(1)
        time.sleep(5)

    def send(s, c, w=0.3):
        k = socket.socket(socket.AF_UNIX)
        k.connect(s.sock)
        k.sendall((c + '\n').encode())
        k.settimeout(2)
        out = b''
        try:
            out = k.recv(65536)
        except Exception:
            pass
        k.close()
        time.sleep(w)
        return out.decode(errors='replace')

    def bounds(s, i):
        r = s.send(f'bounds {i}', 0.1).split()
        try:
            b = [float(v) for v in r[-4:]]
            return b if b[2] > 0 else None
        except Exception:
            return None

    def press(s, i, button=0, w=0.8):
        b = s.bounds(i)
        if not b: return False
        x, y = b[0] + b[2] / 2, b[1] + b[3] / 2
        s.send(f'move {x} {y}', 0.1)
        s.send(f'press {x} {y} {button}', 0.1)
        s.send(f'release {x} {y} {button}', 0.1)
        s.send(f'release {x} {y} {button}', w)
        return True

    # the state now: panes and instances from the log's dump
    def state(s):
        n = os.path.getsize(s.log) if os.path.exists(s.log) else 0
        s.send('app dump', 0.4)
        with open(s.log, errors='replace') as f:
            f.seek(n)
            lines = [l.split('STATE ', 1)[1].strip()
                     for l in f if 'STATE ' in l]
        st = {'panes': [], 'insts': [], 'active': -1}
        for l in lines:
            w = l.split()
            if w[0] == 'begin': st['active'] = int(w[4])
            if w[0] == 'pane':
                d = dict(zip(w[2::2], w[3::2])); d['n'] = int(w[1])
                st['panes'].append(d)
            if w[0] == 'inst':
                d = dict(zip(w[2::2], w[3::2])); d['iid'] = int(w[1])
                st['insts'].append(d)
        return st

    # the box's processes: its own session, and any whose environment
    # names its folder (its args never do)
    def close(s):
        try: os.killpg(s.proc.pid, 9)
        except Exception: pass
        for e in os.listdir('/proc'):
            if not e.isdigit(): continue
            try:
                env = open(f'/proc/{e}/environ', 'rb').read()
            except Exception:
                continue
            if s.dir.encode() in env:
                try: os.kill(int(e), 9)
                except Exception: pass
        time.sleep(1)
        subprocess.run(['rm', '-rf', s.dir])

fails = []
def check(name, st, consoles, at_least=False):
    run = [i for i in st['insts'] if i['running'] == 'true'
           and i['app'].endswith('/console')]
    bad = []
    if (len(run) < consoles) if at_least else (len(run) != consoles):
        bad.append(f'{len(run)} consoles running, {consoles} expected')
    for i in run:
        if int(i['shown']) < 0:
            bad.append(f'console {i["iid"]} in no window')
    for p in st['panes']:
        if p['src'].endswith('/console/console.ag') and p['app'] == 'false':
            bad.append(f'window {p["n"]} shows the console source')
    print(('ok   ' if not bad else 'FAIL ') + name
          + ('' if not bad else ': ' + '; '.join(bad)))
    if bad: fails.append(name)

def console_entry(b):
    for i in b.state()['insts']:
        if i['running'] == 'true' and i['app'].endswith('/console'):
            return f'run_{i["iid"]}'
    return None

# four consoles as the user opens them: the card's launch, then the
# dock entry's menu, New instance (the row after its instances)
def four_then_close(b):
    b.send('app launch console', 6)
    check('first console', b.state(), 1)
    for k in range(1, 4):
        ent = console_entry(b)
        if not ent or not b.press(ent, 1, 1.0):
            print('FAIL no console entry in the dock'); fails.append('dock')
            return
        b.press(f'row{k}', 0, 0.2)
        # through its start: every window kept at every look
        for t in range(12):
            check(f'console {k + 1} starting, look {t}', b.state(), k,
                  True)
            time.sleep(0.3)
        check(f'console {k + 1} up', b.state(), k + 1)
    # the close the bar shows (the window in view), as pressed
    for left in (3, 2, 1, 0):
        st = b.state()
        shown = [p['n'] for p in st['panes'] if p['hidden'] == 'false']
        if st['active'] not in shown:
            print(f'FAIL active window {st["active"]} is not in view')
            fails.append('active')
        on = [n for n in range(len(st['panes']))
              if b.bounds(f'pclose_{n}')]
        if not on:
            print('FAIL no close on the bar'); fails.append('close')
            return
        b.press(f'pclose_{on[0]}', 0, 0.0)
        # never the source, not for a moment
        for t in range(8):
            check(f'closing, {left} left, look {t}', b.state(), left,
                  True)
        time.sleep(1.0)
        check(f'closed one, {left} left', b.state(), left)

# a desktop icon's right press: its menu, Open then its instances
# then New instance (the console's icon at 861, 555 on 1280 x 800)
def card_new(b):
    for k in range(2):
        b.send('move 861 555', 0.1)
        b.send('press 861 555 1', 0.1)
        b.send('release 861 555 1', 0.1)
        b.send('release 861 555 1', 0.8)
        # Open, the instances so far, then New instance
        if not b.press(f'row{1 + k}', 0, 6.0):
            print('FAIL no New instance row'); fails.append('card')
            return
        check(f'card New instance {k + 1}', b.state(), k + 1)
        # back to the desktop for the next press: the window aside
        st = b.state()
        b.press(f'pmin_{st["active"]}', 0, 1.0)

# frames a second, drawing every frame (no cap): the desktop, a free
# window standing, moved by its strip, sized by its top grip
def fps_of(b):
    b.send('stats', 1.2)
    w = b.send('stats', 0).split()
    return int(w[w.index('fps') + 1]) if 'fps' in w else 0

def drag_fps(b, x, y, dx, dy):
    b.send(f'move {x} {y}', 0.1); b.send(f'press {x} {y} 0', 0.1)
    stop = []
    def go():
        i = 0
        while not stop:
            i += 1
            b.send(f'move {x + dx * (i % 60)} {y + dy * (i % 60)}', 0.005)
    t = threading.Thread(target=go); t.start()
    f = fps_of(b)
    stop.append(1); t.join()
    b.send(f'release {x} {y} 0', 0.5)
    return f

def speed(b):
    b.send('animate 1', 0.5)
    got = [('desktop', fps_of(b))]
    b.send(f'app nav open {ROOT}/orbiter/Nav.ag', 3)
    g = b.bounds('bar_grab')
    x, y = g[0] + g[2] / 2, g[1] + g[3] / 2
    b.send(f'move {x} {y}', 0.1); b.send(f'press {x} {y} 0', 0.1)
    for i in range(30):
        b.send(f'move {x + i * 3} {y + 10 + i * 6}', 0.02)
    b.send(f'release {x + 90} {y + 200} 0', 1.0)
    got.append(('free window', fps_of(b)))
    p = b.bounds('pane0')
    got.append(('free window moved',
                drag_fps(b, p[0] + p[2] / 2, p[1] + 12, 1, 1)))
    g = b.bounds('grip0_8')
    got.append(('free window sized',
                drag_fps(b, g[0] + g[2] / 2, g[1] + g[3] / 2, 0, -1)))
    for name, f in got:
        ok = f >= 160
        print(('ok   ' if ok else 'FAIL ') + f'{name}: {f} fps')
        if not ok: fails.append(name)

if __name__ == '__main__':
    for case in (four_then_close, card_new, speed):
        b = Box(env='SILVER_HZ=1000') if case == speed else Box()
        try:
            case(b)
        finally:
            b.close()
    print(f'{len(fails)} failed' if fails else 'all passed')
    sys.exit(1 if fails else 0)
