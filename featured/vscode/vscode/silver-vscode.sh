#!/bin/sh
# on a Wayland display when there is one (orbiter's), else X11:
# Chromium's auto hint follows XDG_SESSION_TYPE, not the display
here=$(dirname "$(readlink -f "$0")")
code="$here/../lib/code-oss/bin/code-oss"
if [ -n "$WAYLAND_DISPLAY" ]; then
    exec "$code" --ozone-platform=wayland "$@"
fi
exec "$code" "$@"
