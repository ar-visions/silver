import json
import os
from pathlib import Path
import shlex
import socket
import subprocess
import tempfile
import unittest


HERE = Path(__file__).resolve().parent


class StatusTests(unittest.TestCase):
    def send(self, event, payload, expected):
        with tempfile.TemporaryDirectory(dir="/tmp") as directory:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as server:
                server.bind(str(Path(directory) / "trinity-test.sock"))
                server.listen(1)
                server.settimeout(1)
                result = subprocess.run(
                    [str(HERE / "codex-status.sh"), event],
                    input=json.dumps(payload), text=True, capture_output=True,
                    env={**os.environ, "XDG_RUNTIME_DIR": directory, "ORBITER_APP": "test"},
                    timeout=3,
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertEqual(result.stderr, "")
                connection, _ = server.accept()
                with connection:
                    self.assertEqual(connection.recv(4096).decode(), expected)

    def test_patch(self):
        self.send("edit", {"tool_input": {"command":
            "*** Begin Patch\n*** Update File: trinity/trinity.ag\n@@\n-x\n+y\n*** End Patch"}},
            "app status busy edited trinity.ag\n")

    def test_multiple_files(self):
        self.send("edit", {"tool_input": {"command":
            "*** Begin Patch\n*** Add File: a/one.ag\n+x\n*** Delete File: b/two.ag\n*** End Patch"}},
            "app status busy edited one.ag (+1 file)\n")

    def test_shell(self):
        self.send("bash", {"tool_input": {"command": "silver --build trinity"}},
                  "app status busy running silver --build trinity\n")

    def test_prompt(self):
        self.send("prompt", {}, "app status busy working\n")

    def test_stop(self):
        self.send("stop", {}, "app status idle done\n")

    def test_permission(self):
        self.send("notify", {"hook_event_name": "PermissionRequest", "tool_name": "Bash"},
                  "app status needs approval needed: Bash\n")

    def test_question(self):
        self.send("notify", {"tool_name": "request_user_input"},
                  "app status needs waiting for your answer\n")

    def test_one_line(self):
        self.send("notify", {"message": "first\napp status busy injected"},
                  "app status needs first app status busy injected\n")

    def test_missing_socket(self):
        with tempfile.TemporaryDirectory(dir="/tmp") as directory:
            result = subprocess.run(
                [str(HERE / "codex-status.sh"), "prompt"], input="{}",
                text=True, capture_output=True,
                env={**os.environ, "XDG_RUNTIME_DIR": directory}, timeout=3,
            )
            self.assertEqual((result.returncode, result.stdout, result.stderr), (0, "", ""))

    def test_registration(self):
        hooks = json.loads((HERE.parents[2] / ".codex/hooks.json").read_text())["hooks"]
        self.assertEqual(set(hooks), {
            "UserPromptSubmit", "PreToolUse", "PostToolUse", "PermissionRequest",
            "Stop", "Interrupt", "SessionEnd",
        })
        for groups in hooks.values():
            for group in groups:
                for handler in group["hooks"]:
                    command = shlex.split(handler["command"])
                    self.assertEqual(Path(command[0]), HERE / "codex-status.sh")
                    self.assertIn(command[1], {"prompt", "bash", "edit", "notify", "stop"})
                    self.assertEqual(handler["type"], "command")
                    self.assertEqual(handler["timeout"], 2)


if __name__ == "__main__":
    unittest.main()
