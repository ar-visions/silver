#!/bin/bash
# cmd+shift+0 kills star-reader and orbiter, even under a grab
kill_apps() {
  pkill -9 -x star-reader
  pkill -9 -x orbiter
  notify-send 'killed star-reader and orbiter' 2>/dev/null
}
[ "$1" = now ] && { kill_apps; exit 0; }
self=$(readlink -f "$0")
pgrep -fx 'xinput test-xi2 --root' >/dev/null && exit 0
codes() { xmodmap -pke | awk -v k="$1" '$4 == k { print $2 }'; }
cmd=$( (codes Super_L; codes Super_R) | tr "\n" " ")
shift=$( (codes Shift_L; codes Shift_R) | tr "\n" " ")
zero=$(codes 0)
stdbuf -oL xinput test-xi2 --root 2>/dev/null |
awk -v cmd="$cmd" -v shift="$shift" -v zero="$zero" '
  BEGIN {
    split(cmd, a, " "); for (i in a) iscmd[a[i]] = 1
    split(shift, b, " "); for (i in b) isshift[b[i]] = 1
  }
  /RawKeyPress/   { st = 1 }
  /RawKeyRelease/ { st = 0 }
  /^EVENT/ && !/RawKey/ { st = -1 }
  /^ +detail:/ && st >= 0 {
    k = $2; down[k] = st
    if (st && k == zero) {
      c = 0; s = 0
      for (x in down) if (down[x]) { if (x in iscmd) c = 1; if (x in isshift) s = 1 }
      if (c && s) { system("'"$self"' now"); fflush() }
    }
  }'
