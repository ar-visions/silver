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
#include <sqlite3.h>
#include <vector>
#include <chrono>
#include <fcntl.h>
#include <signal.h>

extern char** environ;

HOST_API int agent_codex_session_find(const char* root, const char* database,
                                      char* out, int cap) {
    if (!root || !database || !out || cap < 2) return 0;
    out[0] = 0;
#ifdef __APPLE__
    void* library = dlopen("/usr/lib/libsqlite3.dylib", RTLD_LAZY);
#else
    void* library = dlopen("libsqlite3.so.0", RTLD_LAZY);
#endif
    if (!library) return 0;
    auto open = (decltype(&sqlite3_open_v2))dlsym(library, "sqlite3_open_v2");
    auto exec = (decltype(&sqlite3_exec))dlsym(library, "sqlite3_exec");
    auto close = (decltype(&sqlite3_close))dlsym(library, "sqlite3_close");
    sqlite3* db = nullptr;
    struct Match { std::string root; char* out; int cap; size_t best; } match{root, out, cap, 0};
    while (match.root.size() > 1 && match.root.back() == '/') match.root.pop_back();
    if (open && exec && close && open(database, &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK) {
        auto row = [](void* context, int count, char** values, char**) -> int {
            auto& m = *(Match*)context;
            if (count < 2 || !values[0] || !values[1]) return 0;
            std::string cwd = values[1];
            while (cwd.size() > 1 && cwd.back() == '/') cwd.pop_back();
            size_t n = cwd.size();
            if (n <= m.best || n > m.root.size() || m.root.compare(0, n, cwd) != 0) return 0;
            if (n < m.root.size() && cwd != "/" && m.root[n] != '/') return 0;
            if (strlen(values[0]) >= (size_t)m.cap) return 0;
            strcpy(m.out, values[0]);
            m.best = n;
            return 0;
        };
        int result = exec(db, "SELECT id,cwd FROM threads WHERE archived=0 ORDER BY updated_at DESC",
                          row, &match, nullptr);
        if (result != SQLITE_OK) out[0] = 0;
    }
    if (db && close) close(db);
    dlclose(library);
    return out[0] != 0;
}

static std::string codex_session_database() {
    const char* configured = getenv("CODEX_HOME");
    const char* user_home = getenv("HOME");
    std::string directory = configured && *configured ? configured :
        (user_home ? std::string(user_home) + "/.codex" : "");
    if (directory.empty()) return "";
    DIR* entries = opendir(directory.c_str());
    if (!entries) return "";
    std::string database;
    int newest = -1;
    struct dirent* entry;
    while ((entry = readdir(entries))) {
        int version;
        char suffix;
        if (sscanf(entry->d_name, "state_%d.sqlite%c", &version, &suffix) != 1) continue;
        if (version <= newest || strstr(entry->d_name, ".sqlite") == nullptr) continue;
        newest = version;
        database = directory + "/" + entry->d_name;
    }
    closedir(entries);
    return database;
}

struct CodexQueueJob {
    pid_t pid;
    int output;
    std::chrono::steady_clock::time_point deadline;
    std::string reply, error;
};
static std::vector<CodexQueueJob> codex_jobs;

static std::string codex_reply_socket(const char* text) {
    const char* prefix = "on the unix socket ";
    const char* at = nullptr;
    const char* scan = text;
    while ((scan = strstr(scan, prefix))) { at = scan; scan += strlen(prefix); }
    if (!at) return "";
    at += strlen(prefix);
    return std::string(at, strcspn(at, ",\r\n"));
}

static void codex_status(const std::string& path, std::string message, const char* state = "needs") {
    if (strcmp(state, "needs") == 0) fprintf(stderr, "agent: %s\n", message.c_str());
    if (path.empty()) return;
    for (char& ch : message) if (ch == '\n' || ch == '\r') ch = ' ';
    message = std::string("app status ") + state + " " + message.substr(0, 350) + "\n";
    struct sockaddr_un address = {};
    if (path.size() >= sizeof(address.sun_path)) return;
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, path.c_str());
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;
    fcntl(fd, F_SETFL, O_NONBLOCK);
#ifdef SO_NOSIGPIPE
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
    if (connect(fd, (struct sockaddr*)&address, sizeof(address)) == 0) {
#ifdef MSG_NOSIGNAL
        send(fd, message.data(), message.size(), MSG_NOSIGNAL);
#else
        send(fd, message.data(), message.size(), 0);
#endif
    }
    close(fd);
}

static int codex_queue_poll(CodexQueueJob& job) {
    char buffer[1024];
    ssize_t count;
    for (int batch = 0; batch < 8 && (count = read(job.output, buffer, sizeof(buffer))) > 0; ++batch)
        if (job.error.size() < 4096) job.error.append(buffer, count);
    int status = 0;
    pid_t result = waitpid(job.pid, &status, WNOHANG);
    if (result < 0 && errno == EINTR) return 0;
    if (result == 0 && std::chrono::steady_clock::now() < job.deadline) return 0;
    if (result == 0) {
        kill(-job.pid, SIGKILL);
        do { result = waitpid(job.pid, &status, 0); }
        while (result < 0 && errno == EINTR);
        job.error = "Codex queue timed out; press Enter to retry.";
        status = -1;
    }
    for (int batch = 0; batch < 8 && (count = read(job.output, buffer, sizeof(buffer))) > 0; ++batch)
        if (job.error.size() < 4096) job.error.append(buffer, count);
    close(job.output);
    bool ok = result == job.pid && status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (!ok) codex_status(job.reply, job.error.empty() ? "Codex send failed; press Enter to retry." : job.error);
    if (ok) codex_status(job.reply, "Queued for Codex; runs after its current turn.", "note");
    return ok ? 1 : -1;
}

HOST_API int agent_codex_poll() {
    for (size_t i = 0; i < codex_jobs.size();) {
        if (codex_queue_poll(codex_jobs[i])) codex_jobs.erase(codex_jobs.begin() + i);
        else ++i;
    }
    return (int)codex_jobs.size();
}

static int codex_post(const char* root, const char* socket_path,
                      const char* session, const char* text, bool asynchronous) {

    if (!text) return 0;
    if (!session || !*session) session = getenv("CODEX_THREAD_ID");
    char discovered[128];
    if (!session || !*session) {
        std::string database = codex_session_database();
        if (!agent_codex_session_find(root, database.c_str(), discovered, sizeof(discovered))) {
            codex_status(codex_reply_socket(text), "No Codex session for this directory; configure the connection session.");
            return 0;
        }
        session = discovered;
    }
    std::string remote = socket_path && *socket_path ? std::string("unix://") + socket_path : "";
    const char* image = nullptr;
    const char* scan = text;
    while ((scan = strstr(scan, "\n\nScreenshot: "))) {
        scan += strlen("\n\nScreenshot: ");
        image = scan;
    }
    if (!image && strncmp(text, "Screenshot: ", 12) == 0) image = text + 12;
    std::string message = text;
    if (image && *image)
        message += "\n\nOpen the Screenshot path with your image tool before replying.";
    const char* args[14] = { "codex", "queue" };
    int n = 2;
    if (!remote.empty()) { args[n++] = "--remote"; args[n++] = remote.c_str(); }
    args[n++] = "--thread"; args[n++] = session;
    args[n++] = "--message"; args[n++] = message.c_str();
    if (root && *root) { args[n++] = "--cd"; args[n++] = root; }
    args[n] = nullptr;
    int pipes[2];
    if (pipe(pipes) != 0) return 0;
    fcntl(pipes[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipes[1], F_SETFD, FD_CLOEXEC);
    fcntl(pipes[0], F_SETFL, O_NONBLOCK);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, pipes[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipes[0]);
    posix_spawn_file_actions_addclose(&actions, pipes[1]);
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attributes, 0);
    pid_t pid;
    int error = posix_spawnp(&pid, args[0], &actions, &attributes,
        const_cast<char**>(args), environ);
#ifdef __APPLE__
    if (error == ENOENT) {
        const char* home = getenv("HOME");
        std::vector<std::string> directories;
        if (home && *home) directories.push_back(std::string(home) + "/Applications");
        directories.push_back("/Applications");
        for (const auto& directory : directories) {
            for (const char* bundle : {"ChatGPT.app", "Codex.app"}) {
                std::string executable = directory + "/" + bundle + "/Contents/Resources/codex";
                if (access(executable.c_str(), X_OK) != 0) continue;
                error = posix_spawn(&pid, executable.c_str(), &actions, &attributes,
                    const_cast<char**>(args), environ);
                if (error != ENOENT) break;
            }
            if (error != ENOENT) break;
        }
    }
#endif
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attributes);
    close(pipes[1]);
    std::string reply = codex_reply_socket(text);
    if (error) {
        close(pipes[0]);
        codex_status(reply, std::string("Cannot run codex: ") + strerror(error));
        return 0;
    }
    CodexQueueJob job{pid, pipes[0], std::chrono::steady_clock::now() +
        std::chrono::seconds(5), reply, ""};
    if (asynchronous) {
        codex_jobs.push_back(std::move(job));
        return 1;
    }
    int result;
    while (!(result = codex_queue_poll(job))) usleep(10000);
    return result == 1;
}

HOST_API int agent_codex_post(const char* root, const char* socket_path,
                              const char* session, const char* text) {
    return codex_post(root, socket_path, session, text, false);
}

HOST_API int agent_codex_post_async(const char* root, const char* socket_path,
                                    const char* session, const char* text) {
    return codex_post(root, socket_path, session, text, true);
}

static int  g_agent_srv = -1;
static int  g_agent_cli = -1;
static char g_agent_buf[8192];
static int  g_agent_len = 0;

// ---- the agent inbox: a Claude Code session's messaging socket ----
// every live session registers itself under ~/.claude/sessions as
// <pid>.json (its cwd and messagingSocketPath); the sibling
// <pid>.<sha>.key carries the peer token its inbox authenticates with.
// the wire is two JSON lines: the auth, then the user message

static int inbox_read(const char* path, char* buf, int cap) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    int n = (int)fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n < 0 ? 0 : n] = 0;
    return n > 0;
}

// "key":"value" -- copied as written; paths and tokens carry no escapes
static int inbox_str(const char* json, const char* key, char* out, int cap) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char* p = strstr(json, pat);
    if (!p) return 0;
    p += strlen(pat);
    int n = 0;
    while (*p && *p != '"' && n < cap - 1) out[n++] = *p++;
    out[n] = 0;
    return n > 0;
}

static long long inbox_num(const char* json, const char* key) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char* p = strstr(json, pat);
    return p ? atoll(p + strlen(pat)) : -1;
}

// the token for session <pid>: its key file beside the registry entry
static void inbox_token(const char* dir, const char* pid, char* out, int cap) {
    out[0] = 0;
    DIR* d = opendir(dir);
    if (!d) return;
    size_t pl = strlen(pid);
    struct dirent* e;
    while ((e = readdir(d))) {
        size_t ln = strlen(e->d_name);
        if (ln < pl + 5 || strncmp(e->d_name, pid, pl) != 0 || e->d_name[pl] != '.'
            || strcmp(e->d_name + ln - 4, ".key") != 0) continue;
        char fp[1024], buf[1024];
        snprintf(fp, sizeof(fp), "%s/%s", dir, e->d_name);
        if (inbox_read(fp, buf, sizeof(buf))) inbox_str(buf, "peerToken", out, cap);
        break;
    }
    closedir(d);
}

// the inbox for an app: `sock` names one outright (the app's agi may say
// so); empty, the registry is searched for the session whose cwd is the
// longest prefix of `root`, ties to the most recently updated. a dead
// session's stale entry is skipped by its missing socket. 1 = found
HOST_API int agent_inbox_find(const char* root, const char* sock,
                              char* sock_out, int sock_cap, char* token_out, int token_cap) {
    const char* home = getenv("HOME");
    if (!home) return 0;
    char dir[768];
    snprintf(dir, sizeof(dir), "%s/.claude/sessions", home);
    char pid[32] = {0};
    if (sock && *sock) {
        // /tmp/cc-socks/<pid>.sock
        const char* b = strrchr(sock, '/');
        b = b ? b + 1 : sock;
        int n = 0;
        while (b[n] >= '0' && b[n] <= '9' && n < 30) { pid[n] = b[n]; n++; }
        pid[n] = 0;
        snprintf(sock_out, sock_cap, "%s", sock);
    } else {
        if (!root) return 0;
        size_t rl = strlen(root);
        while (rl > 1 && root[rl - 1] == '/') rl--;
        DIR* d = opendir(dir);
        if (!d) return 0;
        size_t best = 0;
        long long best_at = -1;
        struct dirent* e;
        while ((e = readdir(d))) {
            size_t ln = strlen(e->d_name);
            if (ln < 6 || strcmp(e->d_name + ln - 5, ".json") != 0) continue;
            char fp[1024], buf[4096], cwd[512], sk[512];
            snprintf(fp, sizeof(fp), "%s/%s", dir, e->d_name);
            if (!inbox_read(fp, buf, sizeof(buf))) continue;
            if (!inbox_str(buf, "cwd", cwd, sizeof(cwd))
             || !inbox_str(buf, "messagingSocketPath", sk, sizeof(sk))) continue;
            size_t cl = strlen(cwd);
            if (cl > rl || strncmp(cwd, root, cl) != 0 || (cl < rl && root[cl] != '/')) continue;
            if (access(sk, F_OK) != 0) continue;
            long long at = inbox_num(buf, "updatedAt");
            if (cl > best || (cl == best && at > best_at)) {
                best    = cl;
                best_at = at;
                snprintf(sock_out, sock_cap, "%s", sk);
                snprintf(pid, sizeof(pid), "%.*s", (int)(ln - 5), e->d_name);
            }
        }
        closedir(d);
        if (!pid[0]) return 0;
    }
    inbox_token(dir, pid, token_out, token_cap);
    return 1;
}

// a JSON string body: quotes, backslashes and control bytes escaped
static int inbox_put(char* out, int cap, int n, const char* s) {
    for (; *s && n < cap - 8; s++) {
        unsigned char c = (unsigned char)*s;
        if      (c == '"' || c == '\\') { out[n++] = '\\'; out[n++] = (char)c; }
        else if (c == '\n')  { out[n++] = '\\'; out[n++] = 'n'; }
        else if (c == '\r')  { out[n++] = '\\'; out[n++] = 'r'; }
        else if (c == '\t')  { out[n++] = '\\'; out[n++] = 't'; }
        else if (c < 0x20)   n += snprintf(out + n, cap - n, "\\u%04x", c);
        else                 out[n++] = (char)c;
    }
    return n;
}

// post one user message. priority "now" interrupts the session's current
// turn, "next" waits for it. 1 = written
HOST_API int agent_inbox_post(const char* sock, const char* token, const char* from,
                              const char* text, const char* priority) {
    if (!sock || !*sock || !text) return 0;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return 0;
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    struct sockaddr_un su;
    memset(&su, 0, sizeof(su));
    su.sun_family = AF_UNIX;
    strncpy(su.sun_path, sock, sizeof(su.sun_path) - 1);
    if (connect(fd, (struct sockaddr*)&su, sizeof(su)) != 0) { close(fd); return 0; }
    int   cap = (int)strlen(text) * 6 + 1024;
    char* buf = (char*)malloc(cap);
    int   n   = 0;
    if (token && *token)
        n += snprintf(buf + n, cap - n, "{\"type\":\"auth\",\"token\":\"%s\"}\n", token);
    n += snprintf(buf + n, cap - n,
        "{\"type\":\"user\",\"from\":\"%s\",\"priority\":\"%s\",\"message\":{\"role\":\"user\",\"content\":\"",
        (from && *from) ? from : "trinity", (priority && *priority) ? priority : "now");
    n  = inbox_put(buf, cap, n, text);
    n += snprintf(buf + n, cap - n, "\"}}\n");
    int ok = write(fd, buf, n) == n;
    free(buf);
    close(fd);
    return ok;
}

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

#else
// windows stubs: the symbols must exist so the module links. every call reports "nothing there".
HOST_API int  host_pid_alive(int pid)                            { return 0; }
HOST_API int  host_pid_stopped(int pid)                          { return 0; }
HOST_API int  agent_sock_open(const char* name)                  { return 0; }
HOST_API int  agent_sock_line(char* out, int cap)                { return 0; }
HOST_API void agent_sock_reply(const char* s)                    { }
HOST_API int  agent_sock_send(const char* nm, const char* ln)    { return 0; }
HOST_API int  agent_sock_ask(const char* nm, const char* ln, char* o, int c) { return 0; }
HOST_API int agent_codex_post(const char*, const char*, const char*, const char*) { return 0; }
HOST_API int agent_codex_post_async(const char*, const char*, const char*, const char*) { return 0; }
HOST_API int agent_codex_poll() { return 0; }
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
