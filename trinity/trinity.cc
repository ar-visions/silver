#include <cstdio>
extern "C" const char* path_share_name();
// windows exports only what is marked; an elf .so exports every symbol,
// so without this an importing module cannot see these at all
#ifdef _WIN32
#define HOST_API extern "C" __attribute__((dllexport))
#else
#define HOST_API extern "C"
#endif

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

// silver cannot store a C handle into a vec slot by index; write it here
HOST_API void handle_slot_set(void** slot, void* value) { *slot = value; }

// what a module hands the host to hold across its own reload: the process and
// this library outlive the swap, the module's objects do not. host_kept gives
// a value back once and forgets it; a key kept twice holds only the newest
#include <cstring>
#define HOST_KEEP 32
static struct { char key[64]; void* value; } g_host_keep[HOST_KEEP];

HOST_API void host_keep(const char* key, void* value) {
    if (!key || !*key) return;
    int free_at = -1;
    for (int i = 0; i < HOST_KEEP; i++) {
        if (g_host_keep[i].value && strcmp(g_host_keep[i].key, key) == 0) { free_at = i; break; }
        if (!g_host_keep[i].value && free_at < 0) free_at = i;
    }
    if (free_at < 0) return;
    strncpy(g_host_keep[free_at].key, key, sizeof(g_host_keep[free_at].key) - 1);
    g_host_keep[free_at].key[sizeof(g_host_keep[free_at].key) - 1] = 0;
    g_host_keep[free_at].value = value;
}

HOST_API void* host_kept(const char* key) {
    if (!key || !*key) return nullptr;
    for (int i = 0; i < HOST_KEEP; i++)
        if (g_host_keep[i].value && strcmp(g_host_keep[i].key, key) == 0) {
            void* v = g_host_keep[i].value;
            g_host_keep[i].value = nullptr;
            return v;
        }
    return nullptr;
}

#if !defined(_WIN32)
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

// is pid alive? distinguishes an app RELOAD (in-process, same pid) from an
// app EXIT (pid gone) so a consumer knows whether to keep running.
#include <signal.h>
#include <stdlib.h>
#include <execinfo.h>
HOST_API int host_pid_alive(int pid) {
    if (pid <= 0) return 0;
    return kill(pid, 0) == 0 ? 1 : 0;
}

// is pid parked in a stop? the host gates a debug launch with SIGSTOP before
// init, and the ide must see that park to release (or attach to) the app.
// /proc on linux, the process table everywhere else — the same question.
#ifndef __linux__
#include <sys/sysctl.h>
#endif
HOST_API int host_pid_stopped(int pid) {
    if (pid <= 0) return 0;
#ifdef __linux__
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    char buf[512];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n < 4) return 0;
    buf[n] = 0;
    // the state letter follows the parenthesised comm, which may hold spaces
    char* rp = strrchr(buf, ')');
    if (!rp || !rp[1] || !rp[2]) return 0;
    return rp[2] == 'T' ? 1 : 0;
#else
    struct kinfo_proc ki;
    size_t sz     = sizeof(ki);
    int    mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid };
    if (sysctl(mib, 4, &ki, &sz, NULL, 0) != 0 || sz == 0) return 0;
    return ki.kp_proc.p_stat == SSTOP ? 1 : 0;
#endif
}


// the log tee is NOT linux-only; it lives past the #endif below
// ===========================================================================
// agent socket — debug builds only (the .ag side gates on `debug`). a LOCAL
// unix stream socket at $XDG_RUNTIME_DIR/trinity-<app>.sock speaking a line
// protocol; the .ag side authors real Events into the same dispatch GLFW
// uses, so an agent driving it is indistinguishable from a mouse.
// ===========================================================================
#include <sys/socket.h>
#include <sys/un.h>
#include <dirent.h>
#include <spawn.h>
#include <sys/wait.h>
#include <string>
#include <dlfcn.h>
#include <vector>
#include <chrono>
#include <fcntl.h>
#include <signal.h>

extern char** environ;

static int  g_agent_srv = -1;
static int  g_agent_cli = -1;
static char g_agent_buf[8192];
static int  g_agent_len = 0;

HOST_API int agent_sock_open(const char* name) {
    if (g_agent_srv >= 0) return 1;
    if (!name || !*name)  return 0;
    const char* rt = getenv("XDG_RUNTIME_DIR");
    char pathb[256];
    snprintf(pathb, sizeof(pathb), "%s/trinity-%s.sock",
             (rt && *rt) ? rt : "/tmp", name);
    unlink(pathb);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return 0;
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    struct sockaddr_un su;
    memset(&su, 0, sizeof(su));
    su.sun_family = AF_UNIX;
    strncpy(su.sun_path, pathb, sizeof(su.sun_path) - 1);
    if (bind(fd, (struct sockaddr*)&su, sizeof(su)) != 0 || listen(fd, 64) != 0) {
        close(fd);
        return 0;
    }
    g_agent_srv = fd;
    return 1;
}

// a peer parked in the debugger has a full backlog: a blocking connect would
// hang the caller for good, so connect without waiting and treat busy as away
static int agent_connect(const char* name) {
    const char* rt = getenv("XDG_RUNTIME_DIR");
    char pathb[256];
    snprintf(pathb, sizeof(pathb), "%s/trinity-%s.sock",
             (rt && *rt) ? rt : "/tmp", name);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    struct sockaddr_un su;
    memset(&su, 0, sizeof(su));
    su.sun_family = AF_UNIX;
    strncpy(su.sun_path, pathb, sizeof(su.sun_path) - 1);
    if (connect(fd, (struct sockaddr*)&su, sizeof(su)) != 0) {
        close(fd);
        return -1;
    }
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl >= 0) fcntl(fd, F_SETFL, fl & ~O_NONBLOCK);
    return fd;
}

// client: ask a running app one line and read its reply. 0 = nobody home
HOST_API int agent_sock_ask(const char* name, const char* line,
                              char* out, int cap) {
    if (!name || !*name || !line || !out || cap < 2) return 0;
    int fd = agent_connect(name);
    if (fd < 0) return 0;
    size_t n = strlen(line);
    if (write(fd, line, n) != (ssize_t)n) { close(fd); return 0; }
    // the app answers on its next frame; wait briefly rather than spin
    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = 250000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int got = 0;
    while (got < cap - 1) {
        int r = (int)recv(fd, out + got, cap - 1 - got, 0);
        if (r <= 0) break;
        got += r;
        if (memchr(out, '\n', got)) break;
    }
    close(fd);
    out[got] = 0;
    return got;
}

// client: hand one line to an app that is already running. 0 = nobody home
HOST_API int agent_sock_send(const char* name, const char* line) {
    if (!name || !*name || !line || !*line) return 0;
    int fd = agent_connect(name);
    if (fd < 0) return 0;
    size_t  n = strlen(line);
    ssize_t w = write(fd, line, n);
    close(fd);
    return (w == (ssize_t)n) ? 1 : 0;
}

// one complete line per call (newline stripped); 0 = nothing pending
HOST_API int agent_sock_line(char* out, int cap) {
    if (g_agent_srv < 0 || !out || cap < 2) return 0;
    if (g_agent_cli < 0) {
        g_agent_cli = accept(g_agent_srv, 0, 0);
        if (g_agent_cli < 0) return 0;
        fcntl(g_agent_cli, F_SETFL, fcntl(g_agent_cli, F_GETFL, 0) | O_NONBLOCK);
        fcntl(g_agent_cli, F_SETFD, FD_CLOEXEC);
        g_agent_len = 0;
    }
    for (;;) {
        for (int i = 0; i < g_agent_len; i++) {
            if (g_agent_buf[i] != '\n') continue;
            int n = (i < cap - 1) ? i : cap - 1;
            memcpy(out, g_agent_buf, n);
            out[n] = 0;
            memmove(g_agent_buf, g_agent_buf + i + 1, g_agent_len - i - 1);
            g_agent_len -= i + 1;
            return n;
        }
        int room = (int)sizeof(g_agent_buf) - g_agent_len;
        if (room <= 0) { g_agent_len = 0; return 0; }
        int r = (int)recv(g_agent_cli, g_agent_buf + g_agent_len, room, 0);
        if (r > 0) { g_agent_len += r; continue; }
        if (r == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            close(g_agent_cli);
            g_agent_cli = -1;
            g_agent_len = 0;
        }
        return 0;
    }
}

HOST_API void agent_sock_reply(const char* s) {
    if (g_agent_cli < 0 || !s || !*s) return;
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
    ssize_t w = send(g_agent_cli, s, strlen(s), MSG_NOSIGNAL);
    (void)w;
}


// ---- the agent from the shell: one clean run, streamed ----
// claude -p --output-format stream-json, or codex exec --json, in the
// project folder. each JSON event becomes a status line the app takes
// as on_agent: busy / note / diff / done / needs

struct Json {
    enum Kind { NUL, BOOL, NUM, STR, ARR, OBJ } kind = NUL;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Json> a;
    std::vector<std::pair<std::string, Json>> o;
    const Json& operator[](const char* k) const {
        static Json none;
        for (auto& p : o) if (p.first == k) return p.second;
        return none;
    }
    std::string str() const { return kind == STR ? s : ""; }
};

static void json_ws(const char*& p) { while (*p && isspace((unsigned char)*p)) p++; }

static std::string json_string(const char*& p) {
    std::string r;
    p++;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
            case 'n': r += '\n'; break;
            case 't': r += '\t'; break;
            case 'r': r += '\r'; break;
            case 'b': r += '\b'; break;
            case 'f': r += '\f'; break;
            case 'u': {
                unsigned v = 0;
                for (int i = 1; i <= 4 && p[i]; i++)
                    v = v * 16 + (isdigit((unsigned char)p[i]) ? p[i] - '0' : (tolower(p[i]) - 'a' + 10));
                p += 4;
                // utf-8 for the basic plane; surrogate halves pass as-is
                if (v < 0x80) r += (char)v;
                else if (v < 0x800) { r += (char)(0xC0 | (v >> 6)); r += (char)(0x80 | (v & 0x3F)); }
                else { r += (char)(0xE0 | (v >> 12)); r += (char)(0x80 | ((v >> 6) & 0x3F)); r += (char)(0x80 | (v & 0x3F)); }
                break;
            }
            default: r += *p;
            }
            p++;
        } else
            r += *p++;
    }
    if (*p == '"') p++;
    return r;
}

static Json json_value(const char*& p) {
    Json j;
    json_ws(p);
    if (*p == '{') {
        j.kind = Json::OBJ;
        p++;
        json_ws(p);
        while (*p && *p != '}') {
            json_ws(p);
            if (*p != '"') break;
            std::string k = json_string(p);
            json_ws(p);
            if (*p == ':') p++;
            j.o.emplace_back(k, json_value(p));
            json_ws(p);
            if (*p == ',') p++;
            json_ws(p);
        }
        if (*p == '}') p++;
    } else if (*p == '[') {
        j.kind = Json::ARR;
        p++;
        json_ws(p);
        while (*p && *p != ']') {
            j.a.push_back(json_value(p));
            json_ws(p);
            if (*p == ',') p++;
            json_ws(p);
        }
        if (*p == ']') p++;
    } else if (*p == '"') {
        j.kind = Json::STR;
        j.s = json_string(p);
    } else if (!strncmp(p, "true", 4))  { j.kind = Json::BOOL; j.b = true;  p += 4; }
    else if (!strncmp(p, "false", 5))   { j.kind = Json::BOOL; j.b = false; p += 5; }
    else if (!strncmp(p, "null", 4))    { p += 4; }
    else if (*p) {
        char* e;
        j.kind = Json::NUM;
        j.n = strtod(p, &e);
        p = (e > p) ? e : p + 1;
    }
    return j;
}

struct ShellRun {
    pid_t pid = -1;
    int   out = -1;
    int   in  = -1;       // claude's input: each message of the exchange
    int   log = -1;       // the raw stream and errors, for reading after
    bool  claude = true;
    bool  done = false;       // a done or needs line was queued
    std::string buf, root;
    std::vector<std::string> lines;
};
static ShellRun g_shell;
// codex's session for the exchange: later messages resume it
static std::string g_codex_thread;

// a JSON string literal of s
static std::string json_quote(const std::string& s) {
    std::string r = "\"";
    for (unsigned char c : s) {
        if      (c == '"')  r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); r += b; }
        else r += (char)c;
    }
    return r + "\"";
}

// one user message into the open claude process
static bool shell_write(const std::string& text) {
    std::string m = "{\"type\":\"user\",\"message\":{\"role\":\"user\",\"content\":" +
        json_quote(text) + "}}\n";
    size_t at = 0;
    while (at < m.size()) {
        ssize_t w = write(g_shell.in, m.data() + at, m.size() - at);
        if (w <= 0) return false;
        at += (size_t)w;
    }
    g_shell.done = false;
    return true;
}

static std::string shell_base(const std::string& path) {
    size_t s = path.find_last_of('/');
    return s == std::string::npos ? path : path.substr(s + 1);
}

// the agent's words as notes: a ```diff block repeats the edit's
// own diff entry and is left out; fence lines never show
static void shell_text(const std::string& text) {
    bool fence = false, skip = false;
    size_t at = 0;
    while (at <= text.size()) {
        size_t e = text.find('\n', at);
        std::string ln = text.substr(at, e == std::string::npos ? std::string::npos : e - at);
        size_t lead = ln.find_first_not_of(" \t");
        bool mark = lead != std::string::npos && ln.compare(lead, 3, "```") == 0;
        if (mark) {
            fence = !fence;
            skip  = fence && ln.compare(lead + 3, 4, "diff") == 0;
        } else if (!(fence && skip) && ln.find_first_not_of(" \t\r") != std::string::npos)
            g_shell.lines.push_back("note " + ln.substr(0, 400));
        if (e == std::string::npos) break;
        at = e + 1;
    }
}

// the agent's turn ended; a claude process waits for the next message
static void shell_end() {
    g_shell.lines.push_back("done done");
    g_shell.done = true;
}

// every line of a text as its own status line
static void shell_lines(const char* state, const std::string& text) {
    size_t at = 0;
    while (at <= text.size()) {
        size_t e = text.find('\n', at);
        std::string ln = text.substr(at, e == std::string::npos ? std::string::npos : e - at);
        bool blank = ln.find_first_not_of(" \t\r") == std::string::npos;
        if (!blank) g_shell.lines.push_back(std::string(state) + " " + ln.substr(0, 400));
        if (e == std::string::npos) break;
        at = e + 1;
    }
}

// the diff of one edit, from the tool's own old and new text
static void shell_edit_diff(const Json& in, const std::string& root) {
    std::string fp = in["file_path"].str();
    std::string rel = (!root.empty() && fp.compare(0, root.size(), root) == 0 &&
                       fp.size() > root.size()) ? fp.substr(root.size() + 1) : fp;
    g_shell.lines.push_back("diff diff --git a/" + rel + " b/" + rel);
    std::vector<std::pair<std::string, std::string>> hunks;
    if (in["old_string"].kind == Json::STR || in["new_string"].kind == Json::STR)
        hunks.emplace_back(in["old_string"].str(), in["new_string"].str());
    for (auto& e : in["edits"].a)
        hunks.emplace_back(e["old_string"].str(), e["new_string"].str());
    auto each = [](const std::string& t, const char* mark) {
        size_t at = 0;
        while (at <= t.size()) {
            size_t e = t.find('\n', at);
            g_shell.lines.push_back(std::string("diff ") + mark +
                t.substr(at, e == std::string::npos ? std::string::npos : e - at));
            if (e == std::string::npos) break;
            at = e + 1;
        }
    };
    if (hunks.empty() && in["content"].kind == Json::STR) {
        g_shell.lines.push_back("diff @@ " + shell_base(fp) + ": written @@");
        each(in["content"].str(), "+");
        return;
    }
    auto split = [](const std::string& t) {
        std::vector<std::string> v;
        if (t.empty()) return v;
        size_t at = 0;
        while (at <= t.size()) {
            size_t e = t.find('\n', at);
            v.push_back(t.substr(at, e == std::string::npos ? std::string::npos : e - at));
            if (e == std::string::npos) break;
            at = e + 1;
        }
        return v;
    };
    for (auto& h : hunks) {
        g_shell.lines.push_back("diff @@ " + shell_base(fp) + " @@");
        // lines both sides share at either end are context, not change
        std::vector<std::string> o = split(h.first), n = split(h.second);
        size_t pre = 0, suf = 0;
        while (pre < o.size() && pre < n.size() && o[pre] == n[pre]) pre++;
        while (suf < o.size() - pre && suf < n.size() - pre &&
               o[o.size() - 1 - suf] == n[n.size() - 1 - suf]) suf++;
        for (size_t i = 0; i < pre; i++) g_shell.lines.push_back("diff  " + o[i]);
        for (size_t i = pre; i < o.size() - suf; i++) g_shell.lines.push_back("diff -" + o[i]);
        for (size_t i = pre; i < n.size() - suf; i++) g_shell.lines.push_back("diff +" + n[i]);
        for (size_t i = o.size() - suf; i < o.size(); i++) g_shell.lines.push_back("diff  " + o[i]);
    }
}

static void shell_event(const Json& e) {
    std::string type = e["type"].str();
    if (g_shell.claude) {
        if (type == "system" && e["subtype"].str() == "init") {
            g_shell.lines.push_back("busy working");
        }
        else if (type == "assistant") {
            for (auto& c : e["message"]["content"].a) {
                std::string ct = c["type"].str();
                if (ct == "text")
                    shell_text(c["text"].str());
                else if (ct == "tool_use") {
                    std::string name = c["name"].str();
                    const Json& in = c["input"];
                    if (name == "Edit" || name == "Write" || name == "MultiEdit") {
                        shell_edit_diff(in, g_shell.root);
                        g_shell.lines.push_back("busy " + shell_base(in["file_path"].str()));
                    } else if (name == "Bash") {
                        std::string d = in["description"].str();
                        g_shell.lines.push_back("busy " + (d.empty() ? in["command"].str() : d).substr(0, 400));
                    } else {
                        std::string t = in["file_path"].str();
                        if (t.empty()) t = in["pattern"].str();
                        g_shell.lines.push_back("busy " + name + (t.empty() ? "" : " " + shell_base(t)));
                    }
                }
            }
        } else if (type == "result") {
            if (e["is_error"].b) {
                shell_lines("needs", e["result"].str());
                g_shell.lines.push_back("needs error");
                g_shell.done = true;
            } else
                shell_end();
        }
    } else {
        const Json& it = e["item"];
        std::string it_type = it["type"].str();
        if (type == "thread.started" && !e["thread_id"].str().empty())
            g_codex_thread = e["thread_id"].str();
        else if (type == "turn.started")
            g_shell.lines.push_back("busy working");
        else if (type == "item.started" && it_type == "command_execution")
            g_shell.lines.push_back("busy " + it["command"].str().substr(0, 400));
        else if (type == "item.completed" && it_type == "agent_message")
            shell_text(it["text"].str());
        else if (type == "item.completed" && it_type == "file_change")
            for (auto& c : it["changes"].a)
                g_shell.lines.push_back("busy " + shell_base(c["path"].str()));
        else if (type == "turn.completed")
            shell_end();
        else if (type == "turn.failed" || type == "error") {
            std::string m = e["error"]["message"].str();
            if (m.empty()) m = e["message"].str();
            shell_lines("needs", m.empty() ? std::string("the agent failed") : m);
            g_shell.done = true;
        }
    }
}

// the program on PATH, else where its installer puts it
static std::string shell_tool(const char* name) {
    const char* pv = getenv("PATH");
    std::string all = pv ? pv : "";
    size_t at = 0;
    while (at <= all.size()) {
        size_t e = all.find(':', at);
        std::string d = all.substr(at, e == std::string::npos ? std::string::npos : e - at);
        std::string p = d + "/" + name;
        if (!d.empty() && access(p.c_str(), X_OK) == 0) return p;
        if (e == std::string::npos) break;
        at = e + 1;
    }
    const char* h = getenv("HOME");
    std::string home = h ? h : "";
    for (std::string p : { home + "/.local/bin/" + name, home + "/.claude/local/" + name,
                           std::string("/Applications/ChatGPT.app/Contents/Resources/") + name,
                           std::string("/Applications/Codex.app/Contents/Resources/") + name,
                           home + "/Applications/Codex.app/Contents/Resources/" + name })
        if (access(p.c_str(), X_OK) == 0) return p;
    return "";
}

HOST_API void agent_shell_stop() {
    if (g_shell.in >= 0) close(g_shell.in);
    if (g_shell.pid > 0) {
        kill(-g_shell.pid, SIGTERM);
        int st;
        waitpid(g_shell.pid, &st, 0);
    }
    if (g_shell.out >= 0) close(g_shell.out);
    if (g_shell.log >= 0) close(g_shell.log);
    g_shell = ShellRun();
}

// a new exchange: the last one's agent and session end
HOST_API void agent_shell_new() {
    agent_shell_stop();
    g_codex_thread.clear();
}

// one message of the exchange. claude: into the open process, started
// on the first; codex: a run, resuming the exchange's session after
// the first. 1 = sent
HOST_API int agent_shell_start(const char* agent, const char* root,
                               const char* model, const char* text) {
    if (!agent || !root || !text) return 0;
    bool claude = strcmp(agent, "codex") != 0;
    if (claude && g_shell.claude && g_shell.in >= 0 && g_shell.pid > 0)
        return shell_write(text) ? 1 : 0;
    agent_shell_stop();
    std::string exe = shell_tool(claude ? "claude" : "codex");
    if (exe.empty()) return 0;
    // a screenshot rides along as an image for codex; claude opens the path
    std::string image;
    const char* sc = strstr(text, "Screenshot: ");
    if (sc) { sc += 12; image = std::string(sc, strcspn(sc, "\r\n")); }
    std::vector<std::string> args = { exe };
    if (claude) {
        // messages arrive on its input for as long as the exchange lasts
        args.insert(args.end(), { "-p", "--input-format", "stream-json",
            "--output-format", "stream-json", "--verbose",
            "--permission-mode", "acceptEdits", "--no-session-persistence" });
        if (model && *model) args.insert(args.end(), { "--model", model });
    } else {
        if (g_codex_thread.empty())
            args.insert(args.end(), { "exec", "--json", "-s", "workspace-write", "-C", root });
        else
            args.insert(args.end(), { "exec", "resume", g_codex_thread, "--json" });
        if (model && *model) args.insert(args.end(), { "-m", model });
        if (!image.empty()) args.insert(args.end(), { "-i", image });
        args.push_back(text);
    }
    std::vector<char*> argv;
    for (auto& s : args) argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);
    // <SILVER_LOG_DIR or /tmp>/agent-shell.log: the last run as it went
    const char* ld = getenv("SILVER_LOG_DIR");
    std::string lp = std::string((ld && *ld) ? ld : "/tmp") + "/agent-shell.log";
    int lfd = open(lp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (lfd >= 0) {
        std::string head = "run: " + exe + " in " + root + "\n";
        (void)!write(lfd, head.data(), head.size());
    }
    int pipes[2], inp[2] = { -1, -1 };
    if (pipe(pipes) != 0) return 0;
    fcntl(pipes[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipes[0], F_SETFL, O_NONBLOCK);
    if (claude && pipe(inp) != 0) { close(pipes[0]); close(pipes[1]); return 0; }
    if (claude) fcntl(inp[1], F_SETFD, FD_CLOEXEC);
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    if (claude) {
        posix_spawn_file_actions_adddup2(&fa, inp[0], STDIN_FILENO);
        posix_spawn_file_actions_addclose(&fa, inp[0]);
    } else
        posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&fa, pipes[1], STDOUT_FILENO);
    if (lfd >= 0) posix_spawn_file_actions_adddup2(&fa, lfd, STDERR_FILENO);
    else posix_spawn_file_actions_addopen(&fa, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addclose(&fa, pipes[0]);
    posix_spawn_file_actions_addclose(&fa, pipes[1]);
    posix_spawn_file_actions_addchdir_np(&fa, root);
    posix_spawnattr_t at;
    posix_spawnattr_init(&at);
    posix_spawnattr_setflags(&at, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&at, 0);
    pid_t pid;
    int err = posix_spawn(&pid, argv[0], &fa, &at, argv.data(), environ);
    posix_spawn_file_actions_destroy(&fa);
    posix_spawnattr_destroy(&at);
    close(pipes[1]);
    if (claude) close(inp[0]);
    if (err) {
        close(pipes[0]);
        if (claude) close(inp[1]);
        if (lfd >= 0) close(lfd);
        return 0;
    }
    g_shell.in     = claude ? inp[1] : -1;
    g_shell.log    = lfd;
    g_shell.pid    = pid;
    g_shell.out    = pipes[0];
    g_shell.claude = claude;
    g_shell.root   = root;
    return claude ? (shell_write(text) ? 1 : 0) : 1;
}

// the next status line of the run (busy x, note x, diff x, done x,
// needs x) into out; 0 when there is none yet
HOST_API int agent_shell_line(char* out, int cap) {
    if (g_shell.lines.empty() && g_shell.out >= 0) {
        char buf[8192];
        ssize_t n;
        while ((n = read(g_shell.out, buf, sizeof(buf))) > 0) g_shell.buf.append(buf, n);
        size_t nl;
        while ((nl = g_shell.buf.find('\n')) != std::string::npos) {
            std::string ln = g_shell.buf.substr(0, nl);
            g_shell.buf.erase(0, nl + 1);
            if (g_shell.log >= 0) {
                std::string w = ln + "\n";
                (void)!write(g_shell.log, w.data(), w.size());
            }
            const char* p = ln.c_str();
            json_ws(p);
            if (*p == '{') shell_event(json_value(p));
        }
        if (n == 0) {
            // the stream ended: the run is over
            int st = 0;
            waitpid(g_shell.pid, &st, 0);
            close(g_shell.out);
            g_shell.out = -1;
            g_shell.pid = -1;
            if (g_shell.in >= 0) { close(g_shell.in); g_shell.in = -1; }
            if (g_shell.log >= 0) { close(g_shell.log); g_shell.log = -1; }
            if (!g_shell.done)
                g_shell.lines.push_back(WIFEXITED(st) && WEXITSTATUS(st) == 0
                    ? "done done" : "needs the agent stopped before it finished");
        }
    }
    if (g_shell.lines.empty() || cap < 2) return 0;
    snprintf(out, cap, "%s", g_shell.lines.front().c_str());
    g_shell.lines.erase(g_shell.lines.begin());
    return 1;
}

#else
// windows stubs: the symbols must exist so the module links. every call reports "nothing there".
HOST_API int  host_pid_alive(int pid)                            { return 0; }
HOST_API int  host_pid_stopped(int pid)                          { return 0; }
HOST_API int  agent_sock_open(const char* name)                  { return 0; }
HOST_API int  agent_sock_line(char* out, int cap)                { return 0; }
HOST_API void agent_sock_reply(const char* s)                    { }
HOST_API int  agent_sock_send(const char* nm, const char* ln)    { return 0; }
HOST_API int  agent_sock_ask(const char* nm, const char* ln, char* o, int c) { return 0; }
HOST_API void agent_shell_stop() { }
HOST_API void agent_shell_new() { }
HOST_API int  agent_shell_start(const char*, const char*, const char*, const char*) { return 0; }
HOST_API int  agent_shell_line(char* out, int cap) { return 0; }
#endif

// ===========================================================================
// tee stdout/stderr to <logdir>/<app>.log AND the terminal, so a run's full
// output is readable without pasting. one thread drains a pipe the standard
// streams are redirected into. hosting is linux-only, but this is not: it is
// pipe/dup2/thread, which ports supplies on windows too — and without it a
// windows run leaves no log at all.
#ifdef _WIN32
#include <posix.h>   // supplies the pthread api too, so no <pthread.h> here
// declared here, NOT via <io.h>: that header also declares read/write with the
// crt's own signature, which collides with the ones posix.h just gave us
HOST_API intptr_t _get_osfhandle(int fd);
#else
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
extern char** environ;   // macOS declares it nowhere public
#endif
#include <stdlib.h>
#include <string.h>

static int  g_log_real = -1;
static int  g_log_file = -1;
static int  g_log_done = 0;
static int  g_log_pipe = -1;

static void* host_log_thread(void* a) {
    int rd = (int)(long)a;
    char buf[8192];
    ssize_t n;
    while ((n = read(rd, buf, sizeof(buf))) > 0) {
        if (g_log_real >= 0) { ssize_t w = write(g_log_real, buf, n); (void)w; }
        if (g_log_file >= 0) { ssize_t w = write(g_log_file, buf, n); (void)w; }
    }
    return 0;
}

// exit() ends the tee thread with whatever is still in the pipe: an expect
// or fault message written moments before. drain it ourselves on the way out
static void host_log_drain(void) {
    if (g_log_pipe < 0) return;
#ifndef _WIN32
    fflush(stdout);
    fflush(stderr);
    fcntl(g_log_pipe, F_SETFL, fcntl(g_log_pipe, F_GETFL) | O_NONBLOCK);
    char buf[8192];
    ssize_t n;
    while ((n = read(g_log_pipe, buf, sizeof(buf))) > 0) {
        if (g_log_real >= 0) { ssize_t w = write(g_log_real, buf, n); (void)w; }
        if (g_log_file >= 0) { ssize_t w = write(g_log_file, buf, n); (void)w; }
    }
#endif
}

HOST_API void host_log_setup(const char* name) {
    if (g_log_done || !name || !*name) return;
    g_log_done = 1;
    // a hosted app appends to the log its supervisor already wrote the build
    // into, and that file is named by the module: only the primary app takes
    // its share name
    const char* slot_env = getenv("SILVER_APP_SLOT");
    const char* app = path_share_name();
    if (app && *app && !(slot_env && *slot_env)) name = app;

    // silver-host publishes the directory it writes its own logs into, so the
    // two agree by construction rather than by two copies of the same rule
    const char* dir = getenv("SILVER_LOG_DIR");
    if (!dir || !*dir) dir = "/tmp";

    // a name taken from the binary carries .exe here; the log is the app's
    char base[128];
    snprintf(base, sizeof(base), "%s", name);
    { char* dot = strrchr(base, '.');
      if (dot && strcmp(dot, ".exe") == 0) *dot = '\0'; }

    const char* slot = getenv("SILVER_APP_SLOT");
    int sl = (slot && *slot) ? atoi(slot) : 0;
    char path[512];
    if (sl > 0) snprintf(path, sizeof(path), "%s/%s.%d.log", dir, base, sl);
    else        snprintf(path, sizeof(path), "%s/%s.log", dir, base);
    // hosted apps (spawned into a slot) APPEND: the supervisor already truncated the
    // log and wrote the build output into it — the console tails this file, so
    // truncating here would erase the compilation output. the primary app truncates.
    int lflags = (slot && *slot) ? (O_WRONLY | O_CREAT | O_APPEND)
                                 : (O_WRONLY | O_CREAT | O_TRUNC);
    g_log_file = open(path, lflags, 0644);
    if (g_log_file < 0) return;

    // DIAG: the whole environment this process actually runs with, written
    // straight to the log before any redirection. run it the way that works
    // and the way that does not, then diff these blocks
    {
        char line[2048];
        int  n = snprintf(line, sizeof(line), "ENVDUMP begin\n");
        (void)write(g_log_file, line, (size_t)n);
        for (char** e = environ; e && *e; e++) {
            n = snprintf(line, sizeof(line), "ENV %s\n", *e);
            (void)write(g_log_file, line, (size_t)n);
        }
        char cw[1024];
        if (getcwd(cw, sizeof(cw))) {
            n = snprintf(line, sizeof(line), "ENV_CWD %s\n", cw);
            (void)write(g_log_file, line, (size_t)n);
        }
        n = snprintf(line, sizeof(line), "ENVDUMP end\n");
        (void)write(g_log_file, line, (size_t)n);
    }

    // a windows GUI app can start with no usable stdout at all. there is then
    // nothing to tee TO, and a pipe would only swallow the output: point the
    // standard streams straight at the log instead, so prints survive. dup on
    // a dead fd also trips the crt's parameter check rather than returning -1
    int have_out = 1;
#ifdef _WIN32
    // < 0 covers BOTH answers: -1 is a bad fd, -2 is a live fd with nothing
    // behind it -- which is what a /SUBSYSTEM:WINDOWS app actually reports.
    // testing only for -1 concluded we had a console when we had none
    have_out = _get_osfhandle(STDOUT_FILENO) >= 0;
    // silver tails this log to the console itself; writing to our own stdout
    // as well prints everything twice whenever we DO inherit a usable one
    if (getenv("SILVER_LOG_TAIL")) have_out = 0;
#endif
    if (!have_out) {
        dup2(g_log_file, STDOUT_FILENO);
        dup2(g_log_file, STDERR_FILENO);
        setvbuf(stdout, 0, _IONBF, 0);
        setvbuf(stderr, 0, _IONBF, 0);
        return;
    }

    int pf[2];
    if (pipe(pf) != 0) { close(g_log_file); g_log_file = -1; return; }
    g_log_real = dup(STDOUT_FILENO);
    dup2(pf[1], STDOUT_FILENO);
    dup2(pf[1], STDERR_FILENO);
    close(pf[1]);
    // unbuffered, NOT line-buffered: the msvc crt rejects a null buffer with a
    // zero size for _IOLBF (posix allows it) and faults the parameter check,
    // and it treats _IOLBF as fully buffered anyway -- which a tee must not be
    setvbuf(stdout, 0, _IONBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);
    pthread_t t;
    pthread_create(&t, 0, host_log_thread, (void*)(long)pf[0]);
    pthread_detach(t);
    g_log_pipe = pf[0];
    atexit(host_log_drain);
}

// glsl -> spir-v in process: ios cannot exec glslangValidator, and no
// platform should need it on disk. stage comes from the file's extension
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#include <cstring>
#include <cstdlib>
#include <string>

static glslang_stage_t glsl_stage(const char* path) {
    const char* dot = strrchr(path, '.');
    std::string e = dot ? dot + 1 : "";
    if (e == "vert") return GLSLANG_STAGE_VERTEX;
    if (e == "frag") return GLSLANG_STAGE_FRAGMENT;
    if (e == "comp") return GLSLANG_STAGE_COMPUTE;
    if (e == "geom") return GLSLANG_STAGE_GEOMETRY;
    if (e == "tesc") return GLSLANG_STAGE_TESSCONTROL;
    if (e == "tese") return GLSLANG_STAGE_TESSEVALUATION;
    return GLSLANG_STAGE_VERTEX;
}

HOST_API int trinity_glsl_to_spv(const char* src_path, const char* spv_path) {
    FILE* f = fopen(src_path, "rb");
    if (!f) { fprintf(stderr, "glsl: cannot read %s\n", src_path); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    std::string src((size_t)n, '\0');
    fread(&src[0], 1, (size_t)n, f);
    fclose(f);

    static bool inited = false;
    if (!inited) { glslang_initialize_process(); inited = true; }

    glslang_input_t in = {};
    in.language                          = GLSLANG_SOURCE_GLSL;
    in.stage                             = glsl_stage(src_path);
    in.client                            = GLSLANG_CLIENT_VULKAN;
    in.client_version                    = GLSLANG_TARGET_VULKAN_1_2;
    in.target_language                   = GLSLANG_TARGET_SPV;
    in.target_language_version           = GLSLANG_TARGET_SPV_1_5;
    in.code                              = src.c_str();
    in.default_version                   = 100;
    in.default_profile                   = GLSLANG_NO_PROFILE;
    in.force_default_version_and_profile = 0;
    in.forward_compatible                = 0;
    in.messages                          = GLSLANG_MSG_DEFAULT_BIT;
    in.resource                          = glslang_default_resource();

    glslang_shader_t* sh = glslang_shader_create(&in);
    if (!glslang_shader_preprocess(sh, &in) || !glslang_shader_parse(sh, &in)) {
        fprintf(stderr, "glsl: %s\n%s\n%s\n", src_path, glslang_shader_get_info_log(sh),
            glslang_shader_get_info_debug_log(sh));
        glslang_shader_delete(sh);
        return 1;
    }
    glslang_program_t* pr = glslang_program_create();
    glslang_program_add_shader(pr, sh);
    if (!glslang_program_link(pr, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
        fprintf(stderr, "glsl link: %s\n%s\n", src_path, glslang_program_get_info_log(pr));
        glslang_program_delete(pr); glslang_shader_delete(sh);
        return 1;
    }
    glslang_program_SPIRV_generate(pr, in.stage);
    size_t words = glslang_program_SPIRV_get_size(pr);
    FILE* o = fopen(spv_path, "wb");
    if (!o) { glslang_program_delete(pr); glslang_shader_delete(sh); return 1; }
    fwrite(glslang_program_SPIRV_get_ptr(pr), 4, words, o);
    fclose(o);
    glslang_program_delete(pr);
    glslang_shader_delete(sh);
    return 0;
}
