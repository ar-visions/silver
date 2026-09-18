import ctypes
import json
import os
from pathlib import Path
import sqlite3
import socket
import time
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]


class CodexDeliveryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        suffix = "dylib" if sys.platform == "darwin" else "so"
        cls.lib = ctypes.CDLL(str(ROOT / f"install/build/libsilver-trinity.{suffix}"))
        cls.post = cls.lib.agent_codex_post
        cls.post.argtypes = [ctypes.c_char_p] * 4
        cls.post.restype = ctypes.c_int

    def queue(self, message, remote="", exit_code=0):
        with tempfile.TemporaryDirectory(dir="/tmp") as directory:
            output = Path(directory) / "args.json"
            command = Path(directory) / "codex"
            command.write_text(
                f"#!{sys.executable}\nimport json,sys\n"
                f"open({str(output)!r}, 'w').write(json.dumps(sys.argv[1:]))\n"
                f"sys.exit({exit_code})\n"
            )
            command.chmod(0o700)
            with patch.dict(os.environ, {"PATH": directory + os.pathsep + os.environ.get("PATH", "")}):
                result = self.post(b"/tmp/game", remote.encode(), b"test-session", message.encode())
            arguments = json.loads(output.read_text()) if output.exists() else None
            return result, arguments

    def test_local_queue(self):
        result, args = self.queue("Fix this\n\nFile: /tmp/game.ag:9")
        self.assertEqual(result, 1)
        self.assertEqual(args, ["queue", "--thread", "test-session", "--message",
                               "Fix this\n\nFile: /tmp/game.ag:9", "--cd", "/tmp/game"])

    def test_screenshot_with_reply_contract(self):
        message = "Fix this\n\nScreenshot: /tmp/crop with spaces.png\n\nReply through this app as well"
        result, args = self.queue(message)
        self.assertEqual(result, 1)
        self.assertNotIn("--image", args)
        self.assertEqual(args[args.index("--message") + 1], message +
                         "\n\nOpen the Screenshot path with your image tool before replying.")

    def test_remote_is_optional(self):
        result, args = self.queue("Hello", "/tmp/explicit-codex.sock")
        self.assertEqual(result, 1)
        self.assertEqual(args[args.index("--remote") + 1], "unix:///tmp/explicit-codex.sock")

    def test_failure_is_returned(self):
        self.assertEqual(self.queue("Hello", exit_code=7)[0], 0)

    @unittest.skipUnless(sys.platform == "darwin", "macOS desktop install")
    def test_desktop_install_without_path(self):
        with tempfile.TemporaryDirectory(dir="/tmp") as directory:
            output = Path(directory) / "args.json"
            command = Path(directory) / "Applications/ChatGPT.app/Contents/Resources/codex"
            command.parent.mkdir(parents=True)
            command.write_text(f"#!{sys.executable}\nimport json,sys\n"
                               f"open({str(output)!r}, 'w').write(json.dumps(sys.argv[1:]))\n")
            command.chmod(0o700)
            with patch.dict(os.environ, {"HOME": directory, "PATH": "/usr/bin:/bin"}):
                result = self.post(b"/tmp/game", b"", b"test-session", b"Hello")
            self.assertEqual(result, 1)
            self.assertEqual(json.loads(output.read_text()),
                             ["queue", "--thread", "test-session", "--message", "Hello", "--cd", "/tmp/game"])

    def test_async_timeout_and_error_reply(self):
        start = self.lib.agent_codex_post_async
        start.argtypes = [ctypes.c_char_p] * 4
        poll = self.lib.agent_codex_poll
        with tempfile.TemporaryDirectory(dir="/tmp") as directory:
            endpoint = str(Path(directory) / "reply.sock")
            command = Path(directory) / "codex"
            command.write_text(f"#!{sys.executable}\nimport time\ntime.sleep(30)\n")
            command.chmod(0o700)
            with socket.socket(socket.AF_UNIX) as server:
                server.bind(endpoint)
                server.listen(4)
                server.settimeout(1)
                message = f"Hello\nReply through this app as well, on the unix socket {endpoint}, one wire line"
                before = time.monotonic()
                with patch.dict(os.environ, {"PATH": directory + os.pathsep + os.environ.get("PATH", "")}):
                    self.assertEqual(start(b"/tmp", b"", b"test-session", message.encode()), 1)
                self.assertLess(time.monotonic() - before, .5)
                while poll():
                    self.assertLess(time.monotonic() - before, 7)
                    time.sleep(.01)
                connection, _ = server.accept()
                with connection:
                    self.assertIn(b"app status needs Codex queue timed out", connection.recv(4096))
            self.assertLess(time.monotonic() - before, 7)

    def test_error_is_sent_to_exchange(self):
        with tempfile.TemporaryDirectory(dir="/tmp") as directory:
            endpoint = str(Path(directory) / "reply.sock")
            with socket.socket(socket.AF_UNIX) as server:
                server.bind(endpoint)
                server.listen(4)
                server.settimeout(1)
                result, _ = self.queue(f"Reply through this app as well, on the unix socket {endpoint}, one wire line", exit_code=7)
                self.assertEqual(result, 0)
                connection, _ = server.accept()
                with connection:
                    self.assertIn(b"app status needs Codex send failed", connection.recv(4096))

    def test_session_discovery(self):
        find = self.lib.agent_codex_session_find
        find.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
        with tempfile.TemporaryDirectory(dir="/tmp") as directory:
            database = Path(directory) / "state.sqlite"
            with sqlite3.connect(database) as db:
                db.execute("CREATE TABLE threads (id TEXT, cwd TEXT, updated_at INTEGER, archived INTEGER)")
                db.executemany("INSERT INTO threads VALUES (?,?,?,?)", [
                    ("parent", "/tmp", 999, 0),
                    ("old", "/tmp/game", 1, 0),
                    ("active", "/tmp/game", 2, 0),
                    ("archived", "/tmp/game", 5, 1),
                    ("other", "/tmp/games", 999, 0),
                ])
            out = ctypes.create_string_buffer(128)
            self.assertEqual(find(b"/tmp/game/src", os.fsencode(database), out, 128), 1)
            self.assertEqual(out.value, b"active")
            self.assertEqual(find(b"/different", os.fsencode(database), out, 128), 0)


if __name__ == "__main__":
    unittest.main()
