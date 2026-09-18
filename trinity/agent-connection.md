# Choose your agents

List the agents you run in the app's `.agi` file, by adapter name:

```text
agents: [ claude, codex ]
```

The exchange's prompt shows them as a row of buttons; the one lit is
where the first send goes, and the row fades out as the exchange becomes
a conversation. A single `agent: codex` line still works as a list of
one. Omitting both keeps the Claude behavior and `agent_inbox` setting.
Claude configuration and its message format are unchanged.

Codex uses its local `codex queue` command for the existing session.
No shared daemon socket is required. The adapter uses an explicit
session, the inherited `CODEX_THREAD_ID`, or the latest unarchived
session matching the app's directory in Codex's local session index.
The closest parent directory wins before update time. The index is
opened read-only.

The local queue rejects image attachments. Screenshot messages retain
their local path and ask the agent to open it with its image tool.
File references and the complete conversation stay unchanged.
Codex includes the app name and requested priority in the message.
Its queue runs after the current turn; it cannot interrupt an active
turn with `now`. The app displays that queued state. Claude retains
its `now` and `next` behavior.

Queuing runs outside the UI wait path. The app polls the child process,
stops it after five seconds, and shows CLI errors in the exchange.
Press Enter with an empty input to retry the existing conversation.

The adapter uses `codex` on `PATH` first. On macOS it also finds the CLI
inside ChatGPT.app or Codex.app in `~/Applications` or `/Applications`.
Launching an app outside Codex does not require changing its `PATH`.
An explicit connection `socket` opts
into a remote Unix endpoint; it is never assumed for desktop sessions.

Advanced connections use `agent_connection`. Picking the same adapter
retains that connection, including its explicit session and socket. `CodexAgentConnection` accepts optional `session` and
`socket` properties. `SocketAgentConnection` retains the existing inbox
JSON protocol and its optional `token`. Custom connections can derive
from `AgentConnection` and implement `post`.

For an index format the adapter cannot read, select a session explicitly:

```text
agent_connection: CodexAgentConnection
    session: your-existing-session-id
```

Every post includes its reply socket. Trusted Codex hooks forward final
text and diffs; the agent follows the same socket instructions for progress.

Run `python3 agenttest/check.py` for selection, configuration, legacy
socket delivery, and Codex message/image argument checks. Run `python3 agenttest/test_codex.py` for local queuing, screenshot
paths, failure handling, and session discovery using a stand-in CLI.

Message titles can have an optional collapsed description:

```text
app status note Fixed the connection
app status description The app now finds the installed executable.
app status description Existing configuration still works.
```

Each `description` line attaches to the most recent entry. A description
without an entry is ignored. Click the title to expand or collapse its
wrapped description. Messages without descriptions behave as before.
Descriptions do not change the agent's busy or completed state.
Both status helpers accept `description "text"` as well as `note "title"`.
