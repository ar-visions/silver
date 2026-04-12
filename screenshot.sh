#!/bin/bash
# Wait for orbiter window to appear, give it time to finish loading,
# screenshot just its window by name, then close the app.
WID=""
for i in $(seq 1 60); do
    WID=$(xwininfo -root -tree 2>/dev/null | awk '/"orbiter"/ {print $1; exit}')
    [ -n "$WID" ] && break
    sleep 1
done
if [ -z "$WID" ]; then
    echo "orbiter window never appeared" >&2
    exit 1
fi
# window exists — give the app a couple seconds to finish its first frames
sleep 3
import -silent -window "$WID" /tmp/screenshot.png
# close the app
pkill -f "platform/native/debug/orbiter$" 2>/dev/null
