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

// silver-host publishes the directory it writes its own logs into
// (<install>/tmp), so the two agree by construction; read once
#include <cstdlib>
HOST_API const char* host_log_dir() {
    static const char* dir = nullptr;
    if (!dir) { dir = getenv("SILVER_LOG_DIR"); if (!dir || !*dir) dir = "/tmp"; }
    return dir;
}

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
#include <map>
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

// client: a many-line answer, ended by an empty line (agent_sock_reply
// with a trailing blank line); reads until it or the 250 ms wait
HOST_API int agent_sock_ask_block(const char* name, const char* line,
                                  char* out, int cap) {
    if (!name || !*name || !line || !out || cap < 3) return 0;
    int fd = agent_connect(name);
    if (fd < 0) return 0;
    size_t n = strlen(line);
    if (write(fd, line, n) != (ssize_t)n) { close(fd); return 0; }
    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = 250000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int got = 0;
    while (got < cap - 1) {
        int r = (int)recv(fd, out + got, cap - 1 - got, 0);
        if (r <= 0) break;
        got += r;
        if (got >= 2 && out[got - 1] == '\n' && out[got - 2] == '\n') break;
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


// ---- an agent's process: one run, its output by lines ----
// the agent modules (claude, codex, gemini) say what to run and
// read what comes back; this only starts it, feeds its input and
// hands its output over a line at a time

struct ProcRun {
    pid_t pid = -1;
    int   out = -1;
    int   in  = -1;       // its input, when it takes messages there
    int   log = -1;       // the raw stream and errors, for reading after
    int   exit = -1;      // its exit status once it ended
    std::string buf, line;
};
static ProcRun g_proc;
// the other sessions of the exchange, each with its process still
// running; g_proc is the one on screen
static int g_session = 0;
static std::map<int, ProcRun> g_parked;
// the next start's program and arguments, and its environment's
// additions
static std::vector<std::string> g_proc_args, g_proc_env;
// why the last start or write failed
static std::string g_proc_err;

static void proc_stop(ProcRun& s) {
    if (s.in >= 0) close(s.in);
    if (s.pid > 0) {
        kill(-s.pid, SIGTERM);
        int st;
        waitpid(s.pid, &st, 0);
    }
    if (s.out >= 0) close(s.out);
    if (s.log >= 0) close(s.log);
    s = ProcRun();
}

HOST_API void agent_proc_stop() { proc_stop(g_proc); }
HOST_API const char* agent_proc_error() { return g_proc_err.c_str(); }

// the next start: its program first, then each argument
HOST_API void agent_proc_arg(const char* a) { g_proc_args.push_back(a ? a : ""); }
// NAME=value for the next start, over the inherited one
HOST_API void agent_proc_env(const char* kv) { g_proc_env.push_back(kv ? kv : ""); }

// the session on screen: the one showing is parked, its process
// still running, and the chosen one takes its place
HOST_API void agent_proc_select(int id) {
    if (id == g_session) return;
    g_parked[g_session] = std::move(g_proc);
    g_proc = ProcRun();
    auto it = g_parked.find(id);
    if (it != g_parked.end()) { g_proc = std::move(it->second); g_parked.erase(it); }
    g_session = id;
}

// a session closed: its process stops
HOST_API void agent_proc_close(int id) {
    if (id == g_session) { agent_proc_stop(); return; }
    auto it = g_parked.find(id);
    if (it != g_parked.end()) { proc_stop(it->second); g_parked.erase(it); }
}

// the program on PATH, else where its installer puts it
HOST_API const char* agent_tool(const char* name) {
    static std::string found;
    found.clear();
    const char* pv = getenv("PATH");
    std::string all = pv ? pv : "";
    size_t at = 0;
    while (at <= all.size()) {
        size_t e = all.find(':', at);
        std::string d = all.substr(at, e == std::string::npos ? std::string::npos : e - at);
        std::string p = d + "/" + name;
        if (!d.empty() && access(p.c_str(), X_OK) == 0) return (found = p).c_str();
        if (e == std::string::npos) break;
        at = e + 1;
    }
    const char* h = getenv("HOME");
    std::string home = h ? h : "";
    for (std::string p : { home + "/.local/bin/" + name, home + "/.claude/local/" + name,
                           std::string("/Applications/ChatGPT.app/Contents/Resources/") + name,
                           std::string("/Applications/Codex.app/Contents/Resources/") + name,
                           home + "/Applications/Codex.app/Contents/Resources/" + name,
                           // the chatgpt deb carries codex as the mac app does
                           std::string("/usr/lib/chatgpt/resources/") + name })
        if (access(p.c_str(), X_OK) == 0) return (found = p).c_str();
    return found.c_str();
}

// the arguments given start in root; keep_in: its input stays open
// for messages. 1 = started
HOST_API int agent_proc_start(const char* root, int keep_in) {
    g_proc_err.clear();
    std::vector<std::string> args = std::move(g_proc_args);
    std::vector<std::string> extra = std::move(g_proc_env);
    g_proc_args.clear();
    g_proc_env.clear();
    agent_proc_stop();
    if (args.empty() || args[0].empty()) { g_proc_err = "nothing to run"; return 0; }
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    std::vector<std::string> env_s = extra;
    for (char** ep = environ; *ep; ep++) {
        bool over = false;
        for (auto& x : extra) {
            size_t eq = x.find('=');
            if (eq != std::string::npos && strncmp(*ep, x.c_str(), eq + 1) == 0) over = true;
        }
        if (!over) env_s.push_back(*ep);
    }
    std::vector<char*> envp;
    for (auto& e : env_s) envp.push_back(const_cast<char*>(e.c_str()));
    envp.push_back(nullptr);
    // <log dir>/agent-shell[.<session>].log: the session's last run as it went
    std::string lp = std::string(host_log_dir()) + "/agent-shell" +
        (g_session > 0 ? "." + std::to_string(g_session) : std::string()) + ".log";
    int lfd = open(lp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (lfd >= 0) {
        std::string head = "run: " + args[0] + " in " + (root ? root : "") + "\n";
        (void)!write(lfd, head.data(), head.size());
    }
    int pipes[2], inp[2] = { -1, -1 };
    if (pipe(pipes) != 0) { g_proc_err = "no pipe for the agent"; return 0; }
    fcntl(pipes[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipes[0], F_SETFL, O_NONBLOCK);
    if (keep_in && pipe(inp) != 0) {
        close(pipes[0]); close(pipes[1]);
        g_proc_err = "no pipe for the agent";
        return 0;
    }
    if (keep_in) fcntl(inp[1], F_SETFD, FD_CLOEXEC);
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    if (keep_in) {
        posix_spawn_file_actions_adddup2(&fa, inp[0], STDIN_FILENO);
        posix_spawn_file_actions_addclose(&fa, inp[0]);
    } else
        posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&fa, pipes[1], STDOUT_FILENO);
    if (lfd >= 0) posix_spawn_file_actions_adddup2(&fa, lfd, STDERR_FILENO);
    else posix_spawn_file_actions_addopen(&fa, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addclose(&fa, pipes[0]);
    posix_spawn_file_actions_addclose(&fa, pipes[1]);
#if !defined(__ANDROID__) || __ANDROID_API__ >= 34
    // bionic has addchdir_np from api 34 on
    if (root && *root) posix_spawn_file_actions_addchdir_np(&fa, root);
#endif
    posix_spawnattr_t at;
    posix_spawnattr_init(&at);
    posix_spawnattr_setflags(&at, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&at, 0);
    pid_t pid;
    int err = posix_spawn(&pid, argv[0], &fa, &at, argv.data(), envp.data());
    posix_spawn_file_actions_destroy(&fa);
    posix_spawnattr_destroy(&at);
    close(pipes[1]);
    if (keep_in) close(inp[0]);
    if (err) {
        close(pipes[0]);
        if (keep_in) close(inp[1]);
        if (lfd >= 0) close(lfd);
        g_proc_err = args[0] + ": " + strerror(err);
        return 0;
    }
    g_proc.in  = keep_in ? inp[1] : -1;
    g_proc.log = lfd;
    g_proc.pid = pid;
    g_proc.out = pipes[0];
    return 1;
}

// the process runs and takes messages on its input
HOST_API int agent_proc_alive() { return g_proc.in >= 0 && g_proc.pid > 0; }

// text into its input, as it is. 1 = written
HOST_API int agent_proc_write(const char* text) {
    if (g_proc.in < 0 || !text) return 0;
    size_t n = strlen(text), at = 0;
    while (at < n) {
        ssize_t w = write(g_proc.in, text + at, n - at);
        if (w <= 0) { g_proc_err = "the agent stopped taking messages"; return 0; }
        at += (size_t)w;
    }
    return 1;
}

// its next whole line of output, else null; once its output ended,
// agent_proc_exit gives its status (-1 while it runs)
HOST_API const char* agent_proc_line() {
    // a parked session's process keeps working: its output waits in
    // its buffer, read here so a full pipe never stalls it
    for (auto& kv : g_parked) {
        if (kv.second.out < 0) continue;
        char pb[8192];
        ssize_t pn;
        while ((pn = read(kv.second.out, pb, sizeof(pb))) > 0) kv.second.buf.append(pb, pn);
    }
    if (g_proc.out >= 0) {
        char buf[8192];
        ssize_t n;
        while ((n = read(g_proc.out, buf, sizeof(buf))) > 0) g_proc.buf.append(buf, n);
        if (n == 0) {
            // the stream ended: the run is over
            int st = 0;
            waitpid(g_proc.pid, &st, 0);
            close(g_proc.out);
            g_proc.out  = -1;
            g_proc.pid  = -1;
            g_proc.exit = WIFEXITED(st) ? WEXITSTATUS(st) : 128;
            if (g_proc.in >= 0) { close(g_proc.in); g_proc.in = -1; }
        }
    }
    size_t nl = g_proc.buf.find('\n');
    if (nl == std::string::npos) {
        // what was left unterminated at the end is a line too
        if (g_proc.out >= 0 || g_proc.buf.empty()) {
            if (g_proc.out < 0 && g_proc.log >= 0) { close(g_proc.log); g_proc.log = -1; }
            return nullptr;
        }
        nl = g_proc.buf.size();
    }
    g_proc.line = g_proc.buf.substr(0, nl);
    g_proc.buf.erase(0, nl < g_proc.buf.size() ? nl + 1 : nl);
    if (g_proc.log >= 0) {
        std::string w = g_proc.line + "\n";
        (void)!write(g_proc.log, w.data(), w.size());
    }
    return g_proc.line.c_str();
}

HOST_API int agent_proc_exit() { return g_proc.out >= 0 ? -1 : g_proc.exit; }

#else
// windows stubs: the symbols must exist so the module links. every call reports "nothing there".
HOST_API int  host_pid_alive(int pid)                            { return 0; }
HOST_API int  host_pid_stopped(int pid)                          { return 0; }
HOST_API int  agent_sock_open(const char* name)                  { return 0; }
HOST_API int  agent_sock_line(char* out, int cap)                { return 0; }
HOST_API void agent_sock_reply(const char* s)                    { }
HOST_API int  agent_sock_send(const char* nm, const char* ln)    { return 0; }
HOST_API int  agent_sock_ask(const char* nm, const char* ln, char* o, int c) { return 0; }
HOST_API int  agent_sock_ask_block(const char* nm, const char* ln, char* o, int c) { return 0; }
HOST_API void agent_proc_stop() { }
HOST_API void agent_proc_select(int id) { }
HOST_API void agent_proc_close(int id) { }
HOST_API void agent_proc_arg(const char*) { }
HOST_API void agent_proc_env(const char*) { }
HOST_API int  agent_proc_start(const char*, int) { return 0; }
HOST_API int  agent_proc_alive() { return 0; }
HOST_API int  agent_proc_write(const char*) { return 0; }
HOST_API const char* agent_proc_line() { return nullptr; }
HOST_API int  agent_proc_exit() { return 0; }
HOST_API const char* agent_tool(const char*) { return ""; }
HOST_API const char* agent_proc_error() { return "agents run on linux and mac only"; }
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
// standard error's own pipe: its lines reach the log light red
static int  g_err_real = -1;
static int  g_err_pipe = -1;

// a chunk of standard error into the log: each line in light red (the
// console draws the colour), so the two streams read apart
static void host_log_err(const char* buf, ssize_t n) {
    if (g_log_file < 0) return;
    static const char on[] = "\033[38;2;255;140;140m", off[] = "\033[0m";
    ssize_t at = 0;
    while (at < n) {
        ssize_t e = at;
        while (e < n && buf[e] != '\n') e++;
        ssize_t w;
        if (e > at) {
            w = write(g_log_file, on, sizeof(on) - 1);
            w = write(g_log_file, buf + at, e - at);
            w = write(g_log_file, off, sizeof(off) - 1);
        }
        if (e < n) w = write(g_log_file, "\n", 1);
        (void)w;
        at = e + 1;
    }
}

static void* host_err_thread(void* a) {
    int rd = (int)(long)a;
    char buf[8192];
    ssize_t n;
    while ((n = read(rd, buf, sizeof(buf))) > 0) {
        if (g_err_real >= 0) { ssize_t w = write(g_err_real, buf, n); (void)w; }
        host_log_err(buf, n);
    }
    return 0;
}

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
    if (g_err_pipe >= 0) {
        fcntl(g_err_pipe, F_SETFL, fcntl(g_err_pipe, F_GETFL) | O_NONBLOCK);
        while ((n = read(g_err_pipe, buf, sizeof(buf))) > 0) {
            if (g_err_real >= 0) { ssize_t w = write(g_err_real, buf, n); (void)w; }
            host_log_err(buf, n);
        }
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

    const char* dir = host_log_dir();

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
    close(pf[1]);
    // standard error apart: its own pipe and tee, light red in the log
    int ef[2];
    if (pipe(ef) == 0) {
        g_err_real = dup(STDERR_FILENO);
        dup2(ef[1], STDERR_FILENO);
        close(ef[1]);
        pthread_t et;
        pthread_create(&et, 0, host_err_thread, (void*)(long)ef[0]);
        pthread_detach(et);
        g_err_pipe = ef[0];
    } else
        dup2(STDOUT_FILENO, STDERR_FILENO);
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
