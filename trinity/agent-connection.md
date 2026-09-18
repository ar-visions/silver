# Choose your agents

List the agents you run in the app's `.agi` file, by adapter name:

```text
agents: [ claude, codex ]
```

The exchange's prompt shows them as a row of buttons; the one lit is
where the first send goes, and the row fades out as the exchange becomes
a conversation. A single `agent: codex` line still works as a list of
one. Omitting both keeps the Claude behavior and `agent_inbox` setting.
The screenshot prompt and message text are unchanged.

Codex uses `codex queue` over its existing Unix session socket and attaches
that screenshot with `--image`. Launch the app from its Codex session so
it inherits `CODEX_THREAD_ID`. The Codex CLI must be on `PATH`, and its
shared session service must already be running. This does not start an
agent or a backend. Codex queues the message using its own scheduling;
Claude retains its `now` and `next` priorities.

A desktop session without that shared socket cannot receive this route.
Selecting Codex reports that failure; it never sends to Claude instead.

Advanced connections still use `agent_connection`, which takes precedence
when present. `CodexAgentConnection` accepts optional `session` and
`socket` properties. `SocketAgentConnection` retains the existing inbox
JSON protocol and its optional `token`. Custom connections can derive
from `AgentConnection` and implement `post`.

Run `python3 agenttest/check.py` for selection, configuration, legacy
socket delivery, and Codex message/image argument checks. Codex delivery
is tested with a stand-in CLI, without sending to a real agent.
