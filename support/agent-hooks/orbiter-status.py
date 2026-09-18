#!/usr/bin/env python3
# one line of agent status into orbiter's socket, from a hook.
# usage: orbiter-status.py <event>   (edit | bash | prompt | stop | notify | note <text> | diff <line>)
# an edit event also posts the diff of that edit alone (from the tool's own
# old/new text) as diff lines, so the app's box shows exactly what the agent
# changed; a git diff would carry the user's own changes too.
# stdin: the hook's JSON (Claude Code's shape). the line on the wire is
#   app status <state> <text>
# state: busy | idle | done | needs | note | diff. orbiter shows the text in its
# status bar and tilts the avatar while the state is busy.
# socket: $XDG_RUNTIME_DIR or /tmp, trinity-<app>.sock ($ORBITER_APP, default orbiter)
import sys, os, json, socket

event = sys.argv[1] if len(sys.argv) > 1 else 'note'
try:
    data = json.load(sys.stdin)
except Exception:
    data = {}
ti = data.get('tool_input') or {}

if event == 'edit':
    # the line is the thing itself: no 'edited' / 'running' in front of it
    state, text = 'busy', os.path.basename(ti.get('file_path') or '')
elif event == 'bash':
    state, text = 'busy', (ti.get('description') or ti.get('command') or '')
elif event == 'prompt':
    state, text = 'busy', 'working'
elif event == 'stop':
    state, text = 'done', 'done'
elif event == 'notify':
    state, text = 'needs', data.get('message') or 'needs you'
elif event == 'diff':
    # one line of a source diff, as is: its leading +/- and indentation matter
    state, text = 'diff', (sys.argv[2] if len(sys.argv) > 2 else '')
elif event == 'description':
    state, text = 'description', ' '.join(sys.argv[2:])
else:
    state, text = 'note', ' '.join(sys.argv[2:]) or event

if state != 'diff':
    text = ' '.join(text.split())
text = text[:400]
sockdir = os.environ.get('XDG_RUNTIME_DIR') or '/tmp'
name = os.environ.get('ORBITER_APP') or 'orbiter'
path = os.path.join(sockdir, 'trinity-%s.sock' % name)

def send(st, tx):
    # one line per connection; a refused connect (the app's backlog) is retried
    import time
    for attempt in range(20):
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.settimeout(0.5)
            s.connect(path)
            s.sendall(('app status %s %s\n' % (st, tx[:400])).encode())
            s.close()
            return
        except OSError:
            time.sleep(0.02)   # orbiter not up, or busy: try again, then let it go

def edit_diff(inp):
    # the diff of THIS edit alone, from the tool's own input: never a git
    # diff, which holds the user's own uncommitted changes as well
    fp = inp.get('file_path') or ''
    root = os.environ.get('CLAUDE_PROJECT_DIR') or os.getcwd()
    rel = os.path.relpath(fp, root) if fp.startswith(root) else fp
    lines = ['diff --git a/%s b/%s' % (rel, rel)]
    hunks = []
    if 'old_string' in inp or 'new_string' in inp:
        hunks.append((inp.get('old_string') or '', inp.get('new_string') or ''))
    for e in inp.get('edits') or []:
        hunks.append((e.get('old_string') or '', e.get('new_string') or ''))
    if not hunks and 'content' in inp:
        # a whole file written: its first lines, the rest as a count
        body = (inp.get('content') or '').split('\n')
        lines.append('@@ %s: %d lines written @@' % (os.path.basename(fp), len(body)))
        lines += ['+' + l for l in body[:40]]
        if len(body) > 40:
            lines.append('+... %d more lines' % (len(body) - 40))
        return lines
    for old, new in hunks:
        lines.append('@@ %s @@' % os.path.basename(fp))
        lines += ['-' + l for l in old.split('\n')] if old else []
        lines += ['+' + l for l in new.split('\n')] if new else []
    return lines

if event == 'edit' and ti:
    for l in edit_diff(ti):
        send('diff', l)
send(state, text)
