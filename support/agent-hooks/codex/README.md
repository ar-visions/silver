# Codex status hooks

The repository's `.codex/hooks.json` registers the adapter. Review and
trust its definitions once through Codex CLI `/hooks`; Codex skips new
hooks until trusted. Resume the session to load the configuration.

| Event | Orbiter status |
| --- | --- |
| UserPromptSubmit | busy: working |
| PreToolUse: Bash | busy: running the command |
| PostToolUse: apply_patch | busy: edited filename |
| PermissionRequest | needs: approval needed |
| PreToolUse: request_user_input or request_user_input_async | needs: waiting for your answer |
| Stop, Interrupt, SessionEnd | idle |

`codex-status.sh` reads the Codex event JSON. `codex-status.py` translates
patch filenames, shell commands, and approval requests for the existing
`../orbiter-status.py` sender. Claude's sender is unchanged.

Status travels over `$XDG_RUNTIME_DIR/trinity-orbiter.sock`, defaulting
to `/tmp/trinity-orbiter.sock`. `ORBITER_APP` selects a different app.
Each message is one line, with at most 120 characters of status text.
An absent Orbiter is harmless, and hooks produce no model context.

The hook commands contain this checkout's absolute path. Update that
path if you move the checkout. These hooks report status only; prompt
and screenshot delivery into an agent remains Trinity's `agent_post`.

Test with `python3 support/agent-hooks/codex/test_status.py`.

[Codex hook reference](https://learn.chatgpt.com/docs/hooks)
