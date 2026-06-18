#!/usr/bin/env python3
# fast-shot.py — screenshot the screen only while the mouse is moving fast
# (e.g. while you drag the Editor left/right split), so you capture the UI mid-drag.
#
# macOS only. Cursor position comes from osascript (JavaScript-for-Automation + the
# AppKit ObjC bridge, NSEvent.mouseLocation) — no third-party packages. Screens are
# grabbed with the built-in `screencapture`. PNGs land in the directory you run this
# from, named shot_001.png, shot_002.png, ...
#
#   python3 fast-shot.py [speed_px_per_sec] [cooldown_seconds]
#
# Defaults: trigger above ~1200 px/s, at most one shot per 0.30s so a single drag
# yields a handful of frames instead of hundreds. Ctrl-C to stop.
#
# First run, macOS asks for Screen Recording permission for your terminal — allow it
# and re-run.

import os, sys, time, math, subprocess

threshold = float(sys.argv[1]) if len(sys.argv) > 1 else 1200.0   # px/sec
cooldown  = float(sys.argv[2]) if len(sys.argv) > 2 else 0.30     # seconds between shots
poll_s    = 0.01                                                  # seconds between polls

out_dir   = os.getcwd()
JS = 'ObjC.import("AppKit"); var p = $.NSEvent.mouseLocation; p.x + "," + p.y'


def mouse_pos():
    # global cursor position (screen coords, bottom-left origin — only the delta matters).
    try:
        out = subprocess.run(
            ["osascript", "-l", "JavaScript", "-e", JS],
            capture_output=True, text=True, timeout=2).stdout.strip()
    except Exception:
        return None
    if "," not in out:
        return None
    try:
        x, y = out.split(",")
        return (float(x), float(y))
    except ValueError:
        return None


def main():
    sys.stderr.write("fast-shot: watching mouse — drag fast to capture (Ctrl-C to stop)\n")
    sys.stderr.write(f"  trigger : {threshold:g} px/s   cooldown: {cooldown:g}s\n")
    sys.stderr.write(f"  saving  : {out_dir}/shot_NNN.png\n")

    n = 0
    last_shot = 0.0
    prev = None   # (x, y, t)

    while True:
        p = mouse_pos()
        t = time.time()
        if p is not None and prev is not None:
            dt = t - prev[2]
            if dt > 0.0:
                dx = p[0] - prev[0]
                dy = p[1] - prev[1]
                speed = math.hypot(dx, dy) / dt          # px/sec
                if speed >= threshold and (t - last_shot) >= cooldown:
                    n += 1
                    f = os.path.join(out_dir, f"shot_{n:03d}.png")
                    subprocess.run(["screencapture", "-x", f])
                    last_shot = t
                    sys.stderr.write(f"  -> shot_{n:03d}.png  ({speed:.0f} px/s)\n")
        if p is not None:
            prev = (p[0], p[1], t)
        time.sleep(poll_s)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.stderr.write("\nstopped.\n")
