#!/bin/sh
# an update's progress to the phone's orbiter: <piece> <of>, or done
for s in /tmp/trinity-orbiter.sock /run/trinity-orbiter.sock; do
    [ -S "$s" ] || continue
    exec python3 -c 'import socket, sys
s = socket.socket(socket.AF_UNIX)
s.connect(sys.argv[1])
s.sendall(("app update " + " ".join(sys.argv[2:]) + "\n").encode())
' "$s" "$@"
done
