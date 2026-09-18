import importlib.util
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import threading
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('status', HERE / 'codex-status.py')
status = importlib.util.module_from_spec(spec)
spec.loader.exec_module(status)


class StatusTests(unittest.TestCase):
    def send(self, event, payload, expected, arguments=()):
        with tempfile.TemporaryDirectory(dir='/tmp') as directory:
            endpoint = str(Path(directory) / 'test.sock')
            received = []
            with socket.socket(socket.AF_UNIX) as server:
                server.bind(endpoint)
                server.listen(64)
                server.settimeout(.1)
                done = threading.Event()
                def read():
                    while not done.is_set():
                        try:
                            connection, _ = server.accept()
                            with connection:
                                data = b''
                                while True:
                                    chunk = connection.recv(4096)
                                    if not chunk: break
                                    data += chunk
                                received.append(data.decode())
                        except socket.timeout:
                            pass
                thread = threading.Thread(target=read)
                thread.start()
                try:
                    result = subprocess.run([str(HERE / 'codex-status.sh'), event, *arguments],
                        input=json.dumps(payload), text=True, capture_output=True,
                        env={**os.environ, 'TRINITY_AGENT_SOCKET': endpoint}, timeout=3)
                    self.assertEqual((result.returncode, result.stdout, result.stderr), (0, '', ''))
                    done.wait(.1)
                finally:
                    done.set()
                    thread.join()
            self.assertEqual(''.join(received), expected)

    def test_patch(self):
        self.send('edit', {'tool_input': {'command':
            '*** Begin Patch\n*** Update File: trinity/trinity.ag\n@@\n-x\n+y\n*** End Patch'}},
            'app status diff diff --git a/trinity/trinity.ag b/trinity/trinity.ag\n'
            'app status diff @@\napp status diff -x\napp status diff +y\n'
            'app status busy edit applied\n')

    def test_multiple_files(self):
        lines = list(status.patch_lines('*** Add File: a.ag\n+x\n*** Update File: b.ag\n@@\n-y\n+z'))
        self.assertEqual(lines, ['diff --git a/a.ag b/a.ag', '+x',
            'diff --git a/b.ag b/b.ag', '@@', '-y', '+z'])

    def test_deleted_file(self):
        with tempfile.TemporaryDirectory(dir='/tmp') as directory:
            Path(directory, 'old.ag').write_text('old line\nsecond line\n')
            data = {'cwd': directory, 'session_id': 'one', 'tool_use_id': 'call',
                'tool_input': {'command': '*** Begin Patch\n*** Delete File: old.ag\n*** End Patch'}}
            with patch.dict(os.environ, {'XDG_RUNTIME_DIR': directory}):
                status.deleted_files('before_edit', data)
                Path(directory, 'old.ag').unlink()
                old = status.deleted_files('edit', data)
                self.assertEqual(list(status.patch_lines(data['tool_input']['command'], old)),
                    ['diff --git a/old.ag b/old.ag', '-old line', '-second line'])
                self.assertEqual(status.deleted_files('edit', data), {})

    def test_failed_edit(self):
        self.assertEqual(list(status.messages('edit', {'tool_response': {'isError': True}})), [])

    def test_final_reply(self):
        self.send('stop', {'last_assistant_message': 'Fixed it.\nTests passed.'},
            'app status note Fixed it.\napp status note Tests passed.\napp status done done\n')

    def test_note(self):
        self.send('note', {}, 'app status note Actual reply\n', ['Actual reply'])
        self.assertEqual(list(status.messages('note', {})), [])

    def test_description(self):
        self.send('description', {}, 'app status description Optional detail\n', ['Optional detail'])

    def test_shell(self):
        self.send('bash', {'tool_input': {'cmd': 'silver --build trinity'}},
            'app status busy running silver --build trinity\n')

    def test_prompt(self):
        self.send('prompt', {}, 'app status busy working\n')

    def test_stop(self):
        self.send('stop', {}, 'app status done done\n')

    def test_permission(self):
        self.send('notify', {'hook_event_name': 'PermissionRequest', 'tool_name': 'Bash'},
            'app status needs approval needed: Bash\n')

    def test_question(self):
        self.send('notify', {}, 'app status needs waiting for your answer\n')

    def test_no_injected_lines(self):
        self.send('notify', {'message': 'first\napp status busy injected'},
            'app status needs first app status busy injected\n')

    def test_unicode_chunks(self):
        text = '🙂' * 250
        chunks = list(status.chunks(text))
        self.assertEqual(''.join(chunks), text)
        self.assertTrue(all(len(x.encode()) <= 400 for x in chunks))

    def test_route_is_per_turn(self):
        with tempfile.TemporaryDirectory(dir='/tmp') as directory:
            with patch.dict(os.environ, {'XDG_RUNTIME_DIR': directory}):
                os.environ.pop('TRINITY_AGENT_SOCKET', None)
                data = {'session_id': 'one', 'turn_id': 'turn1',
                    'prompt': 'Reply through this app as well, on the unix socket /tmp/app.sock, one wire line'}
                self.assertEqual(status.route('prompt', data), '/tmp/app.sock')
                self.assertEqual(status.route('stop', data), '/tmp/app.sock')
                self.assertIsNone(status.route('stop', {**data, 'session_id': 'two'}))
                self.assertIsNone(status.route('stop', {**data, 'turn_id': 'turn2'}))
                self.assertIsNone(status.route('prompt', {**data, 'prompt': 'Terminal work'}))
                self.assertIsNone(status.route('stop', data))

    def test_missing_socket(self):
        with tempfile.TemporaryDirectory(dir='/tmp') as directory:
            result = subprocess.run([str(HERE / 'codex-status.sh'), 'prompt'], input='{}',
                text=True, capture_output=True, timeout=3,
                env={**os.environ, 'TRINITY_AGENT_SOCKET': directory + '/missing.sock'})
            self.assertEqual((result.returncode, result.stdout, result.stderr), (0, '', ''))

    def test_registration(self):
        hooks = json.loads((HERE.parents[2] / '.codex/hooks.json').read_text())['hooks']
        self.assertIn('Stop', hooks)
        self.assertTrue(any(x.get('matcher') == '^apply_patch$' for x in hooks['PostToolUse']))


if __name__ == '__main__':
    unittest.main()
