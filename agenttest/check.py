import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading


root = Path(__file__).resolve().parents[1]
suffix = "dylib" if sys.platform == "darwin" else "so"
library = root / "install/build" / f"libsilver-agenttest.{suffix}"
subprocess.run(
    [str(root / "install/bin/silver"), "--build", "agenttest"],
    cwd=root,
    check=True,
    timeout=300,
)
with tempfile.TemporaryDirectory(prefix="agenttest-", dir="/tmp") as work:
    endpoint = str(Path(work) / "inbox.sock")
    codex_args = Path(work) / "codex-args.json"
    codex = Path(work) / "codex"
    codex.write_text(
        f"#!{sys.executable}\n"
        "import json, os, sys\n"
        "from pathlib import Path\n"
        "Path(os.environ['TRINITY_CODEX_ARGS']).write_text(json.dumps(sys.argv[1:]))\n"
    )
    codex.chmod(0o700)
    messages = []
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as server:
        server.bind(endpoint)
        server.listen(1)
        server.settimeout(30)

        def receive():
            for _ in range(2):
                connection, _ = server.accept()
                connection.settimeout(10)
                with connection, connection.makefile("r") as stream:
                    messages.extend(json.loads(line) for line in stream)

        reader = threading.Thread(target=receive, daemon=True)
        reader.start()
        result = subprocess.run(
            [
                sys.executable,
                "-c",
                "import ctypes, sys; lib = ctypes.CDLL(sys.argv[1]); "
                "lib.check.restype = ctypes.c_bool; "
                "sys.exit(0 if lib.check() else 1)",
                str(library),
            ],
            cwd=root,
            env={
                **os.environ,
                "TRINITY_AGENT_TEST_SOCKET": endpoint,
                "TRINITY_CODEX_ARGS": str(codex_args),
                "PATH": work + os.pathsep + os.environ.get("PATH", ""),
            },
            timeout=180,
        )
        if result.returncode:
            raise SystemExit(result.returncode)
        reader.join(35)
    assert messages == [
        {"type": "auth", "token": "test-token"},
        {
            "type": "user",
            "from": "Game",
            "priority": "now",
            "message": {
                "role": "user",
                "content": "Fix this light\n\nScreenshot: /tmp/crop with spaces.png",
            },
        },
        {
            "type": "user",
            "from": "Game",
            "priority": "next",
            "message": {
                "role": "user",
                "content": "Fix this light\n\nScreenshot: /tmp/crop with spaces.png",
            },
        },
    ], messages
    assert json.loads(codex_args.read_text()) == [
        "queue", "--remote", "unix://" + endpoint,
        "--thread", "test-session",
        "--message", "Fix this light\n\nScreenshot: /tmp/crop with spaces.png",
        "--cd", "/tmp/game", "--image", "/tmp/crop with spaces.png",
    ]
    print("PASS: agent selection, legacy delivery, and Codex text/image arguments")
