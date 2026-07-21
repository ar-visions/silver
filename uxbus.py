#!/usr/bin/env python3
"""trinity app bus client — drive a running silver app (click, dump tree, screenshot).

  ./uxbus.py orbiter tree                 # element tree (agi) -> stdout
  ./uxbus.py orbiter click dock           # click element by id
  ./uxbus.py orbiter click 640 48         # click at x y
  ./uxbus.py orbiter shot /tmp/a.png      # app screenshots itself
  ./uxbus.py orbiter text                 # text grid

wire: AF_UNIX at $XDG_RUNTIME_DIR/trinity/<app>.sock. one 32-byte header
(8 x i32 LE) then `n` payload bytes. word0=type(1 hello,2 message,3 bye)
word1=n(payload len) word2=klen(key len). payload = key + body.
replies are broadcast, so we register our own .sock and listen there.
"""
import os, socket, struct, sys, time

HDR = 32

def bus_dir():
    rd = os.environ.get('XDG_RUNTIME_DIR') or f'/run/user/{os.getuid()}'
    d = os.path.join(rd, 'trinity')
    os.makedirs(d, exist_ok=True)
    return d

def frame(t, key, body):
    key = (key or '').encode(); body = (body or '').encode()
    data = key + body
    hdr = struct.pack('<8i', t, len(data), len(key), 0, 0, 0, 0, 0)
    return hdr + data

def send(app, key, body, want_reply=True, timeout=6.0):
    d = bus_dir()
    me = os.path.join(d, 'agent.sock')
    srv = None
    if want_reply:
        try: os.unlink(me)
        except FileNotFoundError: pass
        srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        srv.bind(me); srv.listen(1); srv.settimeout(timeout)

    c = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    c.connect(os.path.join(d, f'{app}.sock'))
    c.sendall(frame(1, None, 'agent'))          # hello
    c.sendall(frame(2, key, body))              # message

    if not want_reply:
        c.close(); return None
    try:
        conn, _ = srv.accept()
    except socket.timeout:
        c.close(); srv.close(); os.unlink(me); return None
    conn.settimeout(timeout)
    out = None
    deadline = time.time() + timeout
    try:
        while time.time() < deadline:
            hdr = conn.recv(HDR, socket.MSG_WAITALL)
            if len(hdr) < HDR: break
            t, n, klen = struct.unpack('<8i', hdr)[:3]
            data = conn.recv(n, socket.MSG_WAITALL) if n > 0 else b''
            if t != 2: continue
            k = data[:klen].decode(errors='replace')
            b = data[klen:].decode(errors='replace')
            if k == key:
                out = b; break
    finally:
        conn.close(); c.close(); srv.close()
        try: os.unlink(me)
        except FileNotFoundError: pass
    return out

def main():
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(1)
    app, cmd = sys.argv[1], sys.argv[2]
    arg = ' '.join(sys.argv[3:])
    key = {'tree': 'ux_tree', 'click': 'ux_click',
           'shot': 'ux_shot', 'text': 'ux_text'}.get(cmd)
    if not key:
        print(f'unknown command {cmd}'); sys.exit(1)
    r = send(app, key, arg, want_reply=(cmd != 'shot'))
    if cmd == 'shot':
        print(f'requested {arg}')
    elif r is None:
        print('no reply (app not running, or it did not answer)'); sys.exit(2)
    else:
        print(r)

if __name__ == '__main__':
    main()
