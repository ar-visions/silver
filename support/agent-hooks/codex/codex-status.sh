#!/bin/sh
here="$(cd "$(dirname "$0")" && pwd)"
exec /usr/bin/env python3 "$here/codex-status.py" "$@"
