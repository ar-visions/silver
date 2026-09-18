import json
from pathlib import Path
import re
import shlex
import subprocess
import sys


def translate(event, data):
    tool = data.get("tool_input") or {}
    if isinstance(tool, str):
        tool = {"command": tool}
    if not isinstance(tool, dict):
        tool = {}
    command = tool.get("command") or tool.get("cmd") or ""
    if isinstance(command, list):
        command = shlex.join(str(part) for part in command)
    if not isinstance(command, str):
        command = ""
    result = {"tool_input": {"command": command}}
    if event == "edit":
        files = re.findall(r"^\*\*\* (?:Add File|Update File|Delete File|Move to): (.+)$",
                           command, re.MULTILINE)
        files = list(dict.fromkeys(files))
        name = tool.get("file_path")
        if not isinstance(name, str) or not name:
            name = Path(files[0]).name if files else "files"
            if len(files) > 1:
                count = len(files) - 1
                name += f" (+{count} {'file' if count == 1 else 'files'})"
        result["tool_input"]["file_path"] = name
    elif event == "notify":
        message = data.get("message")
        if not isinstance(message, str) or not message:
            if data.get("hook_event_name") == "PermissionRequest":
                message = "approval needed: " + str(data.get("tool_name") or "tool")
            else:
                message = "waiting for your answer"
        result["message"] = message
    return result


def main():
    event = sys.argv[1] if len(sys.argv) > 1 else "note"
    try:
        data = json.load(sys.stdin)
    except (ValueError, OSError):
        return
    if not isinstance(data, dict):
        return
    emitter = Path(__file__).resolve().parent.parent / "orbiter-status.py"
    try:
        subprocess.run(
            [sys.executable, str(emitter), event],
            input=json.dumps(translate(event, data)), text=True,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            timeout=1,
        )
    except (OSError, subprocess.TimeoutExpired):
        pass


if __name__ == "__main__":
    main()
