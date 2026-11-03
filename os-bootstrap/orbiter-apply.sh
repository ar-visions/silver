#!/bin/sh
# an update's files onto the phone: orbiter takes them live, or the
# phone restarts when a library under it changed. $1: the update's tar
tb=$1
log=/src/silver/install/tmp/silver-orbiter.log
# what it decided and why: kept across the restart it may cause
note=/src/silver/install/tmp/apply.log
sock=/run/trinity-orbiter.sock
[ -S "$sock" ] || sock=/tmp/trinity-orbiter.sock

# what orbiter's processes have mapped (its own library swaps live)
for p in $(pgrep -x orbiter); do
    awk '{ print $6 }' "/proc/$p/maps" 2>/dev/null
done | sort -u > /tmp/apply-mapped

# the files whose contents really differ (launchers and shares come
# every time, unchanged)
tar -d -f "$tb" -C / 2>&1 | sed 's/^tar: //' |
    awk -F': ' '/Contents differ|Size differs|No such file/ { print "/" $1 }' |
    sort -u > /tmp/apply-changed

restart=0
while read -r f; do
    case "$f" in */libsilver-orbiter.so) continue ;; esac
    if grep -qxF "$f" /tmp/apply-mapped; then
        echo "$(date -u) restart: $f is in use" >> "$note"
        restart=1
        break
    fi
done < /tmp/apply-changed

# a library in use keeps its old file; the new one is a new file
staged=$(grep -c 'reload staged\|\] reloading (' "$log" 2>/dev/null)
tar --unlink-first --keep-directory-symlink -x -C / -f "$tb" || exit 2
# our libraries' pkg-config files name the PC's fp6 folder
for pc in /src/silver/install/lib/pkgconfig/*.pc \
        /src/silver/install/share/pkgconfig/*.pc; do
    [ -f "$pc" ] && sed -i 's|/src/silver/platform/fp6|/src/silver/install|g' "$pc"
done
[ "$restart" = 1 ] && exit 1

# orbiter's host sees its library change: it stages the swap (or
# swaps); the staged one is applied as ctrl / cmd + shift + R does
n=0
while [ "$n" -lt 40 ]; do
    now=$(grep -c 'reload staged\|\] reloading (' "$log" 2>/dev/null)
    [ "${now:-0}" -gt "${staged:-0}" ] && break
    n=$((n + 1))
    sleep 0.25
done
if [ "$n" -lt 40 ] && grep -q 'reload staged' "$log"; then
    python3 -c 'import socket, sys
for c in ("key 82 1 3", "key 82 0 3"):
    s = socket.socket(socket.AF_UNIX)
    s.connect(sys.argv[1])
    s.sendall((c + "\n").encode())
    s.recv(64)
' "$sock"
fi
echo "$(date -u) live" >> "$note"
exit 0
