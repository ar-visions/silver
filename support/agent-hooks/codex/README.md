# Codex app communication

The app still selects `codex` in its existing agents list. Each message
carries its reply socket. The prompt hook binds that socket to the Codex
session and turn; terminal work and other sessions do not send into it.

The repository's `.codex/hooks.json` registers the hooks. Codex requires
these definitions to be trusted through `/hooks` before running them.
Resume the session after enabling them. No trust checks are bypassed.

| Event | App receives |
| --- | --- |
| UserPromptSubmit | busy: working; records the reply socket |
| PreToolUse: Bash | busy: command |
| PreToolUse: apply_patch | saves deleted file contents |
| PostToolUse: apply_patch | each file's patch lines, then busy |
| PermissionRequest / request_user_input | needs: reason |
| Stop | final assistant text as note lines, then done |
| Interrupt / SessionEnd | done |

Progress commentary still follows the message's reply instructions.
There is no assistant-commentary hook. Final replies use the documented
`last_assistant_message` field rather than reading a transcript.
Manual notes can use `codex-status.sh note "message"` with hook JSON on
stdin, or an explicit `TRINITY_AGENT_SOCKET` environment variable.

Text is split into wire lines of at most 400 UTF-8 bytes. Diff lines
retain their indentation and signs; the existing preview limit is 400
characters per line. Deleted files are captured before apply_patch runs.
Only that tool call's edits are sent; the working tree is not diffed.
An absent app is harmless, and hook work has a bounded time budget.
Claude's hooks and sender are unchanged.

Run `python3 support/agent-hooks/codex/test_status.py`.

[Codex hook reference](https://learn.chatgpt.com/docs/hooks)
