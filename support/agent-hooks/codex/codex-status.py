import hashlib
import json
import os
from pathlib import Path
import re
import socket
import sys
import tempfile
import time


def patch_lines(command, deleted=None):
    active = False
    for line in command.splitlines():
        match = re.match(r"\*\*\* (?:Update|Add|Delete) File: (.+)$", line)
        if match:
            name = match[1]
            yield f"diff --git a/{name} b/{name}"
            active = True
            if line.startswith("*** Delete File: "):
                for old in (deleted or {}).get(name, "").splitlines():
                    yield "-" + old
        elif line.startswith("*** Move to: ") and active:
            yield "rename to " + line[13:]
        elif line.startswith("*** "):
            continue
        elif active and (line.startswith(("+", "-", " ", "@@"))):
            yield line


def messages(event, data, arguments=()):
    tool = data.get("tool_input") or {}
    if isinstance(tool, str):
        tool = {"command": tool}
    if not isinstance(tool, dict):
        tool = {}
    command = tool.get("command") or tool.get("cmd") or ""
    if not isinstance(command, str):
        command = ""
    if event == "edit":
        response = data.get("tool_response")
        if isinstance(response, dict) and response.get("isError"):
            return
        yield from (("diff", line) for line in patch_lines(command, data.get("deleted_files")))
        yield "busy", "edit applied"
    elif event == "stop":
        text = data.get("last_assistant_message")
        if isinstance(text, str):
            yield from (("note", line) for line in text.splitlines() if line.strip())
        yield "done", "done"
    elif event in ("note", "description"):
        text = " ".join(arguments) or data.get("message") or ""
        if isinstance(text, str):
            yield from ((event, line) for line in text.splitlines() if line.strip())
    elif event == "bash":
        yield "busy", "running " + command
    elif event == "prompt":
        yield "busy", "working"
    elif event == "notify":
        text = data.get("message") or (
            "approval needed: " + str(data.get("tool_name") or "tool")
            if data.get("hook_event_name") == "PermissionRequest"
            else "waiting for your answer")
        yield "needs", str(text)


def route(event, data):
    explicit = os.environ.get("TRINITY_AGENT_SOCKET")
    if explicit:
        return explicit
    session = data.get("session_id")
    if not isinstance(session, str) or not session:
        return None
    directory = Path(os.environ.get("XDG_RUNTIME_DIR") or tempfile.gettempdir()) / "trinity-codex-hooks"
    directory.mkdir(mode=0o700, parents=True, exist_ok=True)
    record = directory / (hashlib.sha256(session.encode()).hexdigest() + ".json")
    if event == "prompt":
        prompt = data.get("prompt") or ""
        matches = re.findall(r"on the unix socket ([^\r\n,]+),", prompt) if isinstance(prompt, str) else []
        if not matches:
            record.unlink(missing_ok=True)
            return None
        path = matches[-1]
        record.write_text(json.dumps({"socket": path, "turn": data.get("turn_id")}))
        return path
    try:
        saved = json.loads(record.read_text())
    except (OSError, ValueError):
        return None
    if data.get("turn_id") and saved.get("turn") != data["turn_id"]:
        return None
    if data.get("hook_event_name") == "SessionEnd":
        record.unlink(missing_ok=True)
    return saved.get("socket")


def deleted_files(event, data):
    session = data.get("session_id")
    call = data.get("tool_use_id")
    if not session or not call:
        return {}
    folder = Path(os.environ.get("XDG_RUNTIME_DIR") or tempfile.gettempdir()) / "trinity-codex-hooks"
    folder.mkdir(mode=0o700, parents=True, exist_ok=True)
    key = hashlib.sha256((str(session) + ":" + str(call)).encode()).hexdigest()
    record = folder / (key + ".patch.json")
    if event == "before_edit":
        tool = data.get("tool_input") or {}
        command = tool if isinstance(tool, str) else tool.get("command", "")
        found = {}
        for name in re.findall(r"^\*\*\* Delete File: (.+)$", command, re.MULTILINE):
            try:
                found[name] = (Path(data.get("cwd") or os.getcwd()) / name).read_text()
            except (OSError, UnicodeError):
                pass
        if found:
            record.write_text(json.dumps(found))
        return {}
    try:
        saved = json.loads(record.read_text())
        record.unlink(missing_ok=True)
        return saved
    except (OSError, ValueError):
        return {}


def chunks(text, limit=400):
    part = ""
    for char in text:
        if len((part + char).encode()) > limit:
            yield part
            part = ""
        part += char
    if part:
        yield part


def send(path, state, text, deadline):
    lines = [text[:400]] if state == "diff" else chunks(" ".join(text.split()))
    for line in lines:
        while time.monotonic() < deadline:
            try:
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
                    client.settimeout(min(.2, max(.01, deadline - time.monotonic())))
                    client.connect(path)
                    client.sendall(f"app status {state} {line}\n".encode())
                break
            except OSError:
                time.sleep(.01)
        else:
            return


def main():
    event = sys.argv[1] if len(sys.argv) > 1 else "note"
    try:
        data = json.load(sys.stdin)
    except (ValueError, OSError):
        data = {}
    if not isinstance(data, dict):
        return
    try:
        path = route(event, data)
        if not path:
            return
        if event in ("before_edit", "edit"):
            data["deleted_files"] = deleted_files(event, data)
        deadline = time.monotonic() + 1.5
        for state, text in messages(event, data, sys.argv[2:]):
            send(path, state, text, deadline)
    except OSError:
        pass


if __name__ == "__main__":
    main()
