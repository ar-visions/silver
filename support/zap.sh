#!/bin/bash
# Kalen sees a symptom now: follow the apps until they exit
msg="${*:-slow close}"
D=$(cd "$(dirname "$0")/../install/tmp" && pwd)/zap
mkdir -p $D
out=$D/zap.$(date +%H%M%S).txt
echo "$(date +%T) | $msg | $out" >> $D/inbox
run=${XDG_RUNTIME_DIR:-/tmp}
for sock in $run/trinity-*.sock; do
  [ -S "$sock" ] || continue
  echo "== $(basename $sock) stats" >> $out
  python3 - "$sock" >> $out 2>&1 <<'EOF'
import socket, sys
s = socket.socket(socket.AF_UNIX)
s.settimeout(1.0)
try:
    s.connect(sys.argv[1]); s.sendall(b'stats\n'); print(s.recv(65536).decode(errors='replace'))
except Exception as e:
    print('no answer:', e)
EOF
done
t0=$(date +%s.%N)
for n in $(seq 1 150); do
  echo "@ $(date +%T.%N)" >> $out
  live=0
  for p in /proc/[0-9]*; do
    c=$(cat $p/comm 2>/dev/null) || continue
    case "$c" in orbiter*|aura*|WPE*) ;; *) continue;; esac
    live=1
    echo "$(basename $p) $c" >> $out
    for t in $p/task/*; do
      s=$(cut -d')' -f2- $t/stat 2>/dev/null)
      set -- $s
      echo "  $(basename $t) $(cat $t/comm 2>/dev/null) $1 wchan=$(cat $t/wchan 2>/dev/null) utime=${12} stime=${13}" >> $out
    done
  done
  [ $live = 0 ] && break
  sleep 0.2
done
took=$(echo "$(date +%s.%N) - $t0" | bc)
echo "$(date +%T) | done after ${took}s | $out" >> $D/inbox
echo "zapped: $out"
