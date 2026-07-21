#include <cstdio>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

// ---------------------------------------------------------------------------
// appshare: AF_UNIX + SCM_RIGHTS fd-passing shim. the CMSG_* macros are not
// callable from silver, so the socket plumbing lives here as plain C. all
// messages are a fixed 32-byte frame (8 x u32 words); SLOT also carries one
// dma-buf fd out of band. every fd/socket is non-blocking. the symbols are
// defined on every platform (stubs off linux) so AppLink links everywhere.
// ---------------------------------------------------------------------------
#if defined(__linux__)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#define APPSHARE_MSG_BYTES 32

static void appshare_nonblock(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl >= 0) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

extern "C" int appshare_listen(const char* path) {
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) return -1;
    struct sockaddr_un addr; memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    unlink(path);
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(s); return -1; }
    if (listen(s, 1) < 0) { close(s); return -1; }
    appshare_nonblock(s);
    return s;
}

// non-blocking accept: -1 = none yet / error, else the connected fd
extern "C" int appshare_accept(int s) {
    int c = accept(s, 0, 0);
    if (c < 0) return -1;
    appshare_nonblock(c);
    return c;
}

extern "C" int appshare_connect(const char* path) {
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) return -1;
    struct sockaddr_un addr; memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(s); return -1; }
    appshare_nonblock(s);
    return s;
}

// write one fixed frame. returns 1 ok, 0 peer gone / not draining, -1 error.
// MSG_NOSIGNAL: a peer that exited must read as EPIPE, not SIGPIPE.
// EAGAIN: the socket is FULL because the peer stopped reading (e.g. the app
// hot-reloaded and isn't draining the shell yet). do NOT busy-spin — that was
// a 100%-CPU userspace hang that starved the render/dictation loop. drop the
// frame cleanly at a message boundary (off==0); frames are transient. only a
// partial write (off>0, rare on a 32B message) briefly retries to avoid
// corrupting the stream.
static int appshare_write_all(int s, const void* buf, unsigned len) {
    const char* p = (const char*)buf; unsigned off = 0;
    int spins = 0;
    while (off < len) {
        ssize_t n = send(s, p + off, len - off, MSG_NOSIGNAL);
        if (n > 0) { off += (unsigned)n; spins = 0; continue; }
        if (n == 0) return 0;
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (off == 0) return 0;              // clean drop, nothing sent
            if (++spins > 200) return 0;         // partial: give up ~100ms
            usleep(500);
            continue;
        }
        if (errno == EPIPE) return 0;
        return -1;
    }
    return 1;
}

extern "C" int appshare_send(int s, const void* msg) {
    return appshare_write_all(s, msg, APPSHARE_MSG_BYTES);
}

// send one 32-byte frame + one fd via SCM_RIGHTS. returns 1 ok, 0 peer gone, -1.
extern "C" int appshare_send_fd(int s, int fd, const void* msg) {
    struct iovec io; io.iov_base = (void*)msg; io.iov_len = APPSHARE_MSG_BYTES;
    char cbuf[CMSG_SPACE(sizeof(int))]; memset(cbuf, 0, sizeof(cbuf));
    struct msghdr mh; memset(&mh, 0, sizeof(mh));
    mh.msg_iov = &io; mh.msg_iovlen = 1;
    mh.msg_control = cbuf; mh.msg_controllen = sizeof(cbuf);
    struct cmsghdr* cm = CMSG_FIRSTHDR(&mh);
    cm->cmsg_level = SOL_SOCKET; cm->cmsg_type = SCM_RIGHTS;
    cm->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cm), &fd, sizeof(int));
    for (;;) {
        ssize_t n = sendmsg(s, &mh, MSG_NOSIGNAL);
        if (n > 0) return 1;
        if (n == 0) return 0;
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
        if (errno == EPIPE) return 0;
        return -1;
    }
}

// non-blocking recv of one 32-byte frame. returns 1 got one, 0 none yet,
// -1 peer closed / error. if a fd rides along it is stored in *fd_out (else -1).
extern "C" int appshare_recv_fd(int s, void* buf, int* fd_out) {
    if (fd_out) *fd_out = -1;
    struct iovec io; io.iov_base = buf; io.iov_len = APPSHARE_MSG_BYTES;
    char cbuf[CMSG_SPACE(sizeof(int))]; memset(cbuf, 0, sizeof(cbuf));
    struct msghdr mh; memset(&mh, 0, sizeof(mh));
    mh.msg_iov = &io; mh.msg_iovlen = 1;
    mh.msg_control = cbuf; mh.msg_controllen = sizeof(cbuf);
    ssize_t n;
    for (;;) {
        n = recvmsg(s, &mh, 0);
        if (n >= 0) break;
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }
    if (n == 0) return -1;                 // peer closed
    if (n != APPSHARE_MSG_BYTES) return -1;
    struct cmsghdr* cm = CMSG_FIRSTHDR(&mh);
    if (cm && cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_RIGHTS &&
        cm->cmsg_len == CMSG_LEN(sizeof(int)) && fd_out) {
        memcpy(fd_out, CMSG_DATA(cm), sizeof(int));
    }
    return 1;
}

extern "C" void appshare_close(int fd) { if (fd >= 0) close(fd); }

// one frame then plen payload bytes on the same stream
extern "C" int appshare_send_msg(int s, const void* msg, const void* payload, int plen) {
    int r = appshare_write_all(s, msg, APPSHARE_MSG_BYTES);
    if (r != 1) return r;
    if (plen > 0 && payload)
        return appshare_write_all(s, payload, (unsigned)plen);
    return 1;
}

// spin-read exactly len promised payload bytes; 1 ok, -1 closed
extern "C" int appshare_recv_payload(int s, void* buf, int len) {
    char* p = (char*)buf; int off = 0;
    while (off < len) {
        ssize_t n = read(s, p + off, (size_t)(len - off));
        if (n > 0) { off += (int)n; continue; }
        if (n == 0) return -1;
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
        return -1;
    }
    return 1;
}
// ask the supervising silver-host to bring up crashman (SIGUSR1). guarded on
// the isolate-child marker so an unsupervised app never signals its shell.
#include <signal.h>
#include <stdlib.h>
extern "C" int host_ask_crashman(void) {
    if (!getenv("SILVER_ISOLATE_CHILD")) return -1;
    return kill(getppid(), SIGUSR1);
}

// is pid alive? distinguishes an app RELOAD (in-process, same pid) from an
// app EXIT (pid gone) so the isolate shell knows whether to keep running.
extern "C" int host_pid_alive(int pid) {
    if (pid <= 0) return 0;
    return kill(pid, 0) == 0 ? 1 : 0;
}

// ===========================================================================
// host message channel — ONE WndProc-style interface, no sockets, no scattered
// per-feature function pointers. a single shared-memory region (fixed path, so
// it survives the app's hot-reload) holds two SPSC message rings + one frame
// buffer. the app posts to ring 0 / polls ring 1; crashman posts to ring 1 /
// polls ring 0. each side pumps its inbound ring and switches on msg.type.
// ===========================================================================
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>

enum {
    HM_NONE = 0,
    HM_DICTATE,   // app->shell   a=state (0 idle,1 rec,2 cancel)
    HM_LIVE,      // shell->app   a=0/1 mic live (button blue)
    HM_PRESS,     // app->shell   a=x b=y c=button
    HM_RELEASE,   // app->shell   a=x b=y c=button
    HM_MOVE,      // app->shell   a=x b=y
    HM_KEY,       // app->shell   a=unicode b=state c=mods
    HM_TEXT,      // app->shell   a=codepoint
    HM_FRAME,     // shell->app   a=w b=h c=seq (pixels in shm frame buffer)
    HM_RESIZE,    // app->shell   a=w b=h  (overlay size the shell renders to)
    HM_BYE        // either       overlay dismissed
};

#define HM_SLOTS 1024
typedef struct { int32_t type, a, b, c; } HostMsg;
typedef struct { volatile uint32_t head, tail; HostMsg m[HM_SLOTS]; } HostRing;
#define HM_FRAME_MAX (1920 * 1200 * 4)
typedef struct {
    HostRing to_shell;   // app  -> shell
    HostRing to_app;     // shell -> app
    volatile int32_t fw, fh, fseq;
    uint8_t frame[HM_FRAME_MAX];
} HostShared;

static HostShared* g_hs = 0;
static HostShared* host_shared(void) {
    if (g_hs) return g_hs;
    // the channel is per-session: keyed to the supervisor's pid (handed down
    // as SILVER_ISOLATE_SESSION). no session → not isolated → no channel.
    const char* sess = getenv("SILVER_ISOLATE_SESSION");
    if (!sess || !*sess) return 0;
    char path[128];
    snprintf(path, sizeof(path), "/tmp/silver-crashman-%s.shm", sess);
    int fd = open(path, O_RDWR | O_CREAT, 0600);
    if (fd < 0) return 0;
    if (ftruncate(fd, sizeof(HostShared)) != 0) { close(fd); return 0; }
    void* p = mmap(0, sizeof(HostShared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) return 0;
    g_hs = (HostShared*)p;
    return g_hs;
}

// post a message to a ring (0 = to_shell, 1 = to_app). SPSC: one producer.
extern "C" void host_post(int ring, int type, int a, int b, int c) {
    HostShared* h = host_shared(); if (!h) return;
    HostRing* r = ring ? &h->to_app : &h->to_shell;
    uint32_t head = r->head;
    r->m[head % HM_SLOTS].type = type;
    r->m[head % HM_SLOTS].a = a;
    r->m[head % HM_SLOTS].b = b;
    r->m[head % HM_SLOTS].c = c;
    __sync_synchronize();
    r->head = head + 1;
}

// pop next message from a ring; fills g_last, returns type (0 = empty).
static HostMsg g_last;
extern "C" int host_poll(int ring) {
    HostShared* h = host_shared(); if (!h) return 0;
    HostRing* r = ring ? &h->to_app : &h->to_shell;
    if (r->tail == r->head) { g_last.type = 0; return 0; }
    g_last = r->m[r->tail % HM_SLOTS];
    __sync_synchronize();
    r->tail = r->tail + 1;
    return g_last.type;
}
extern "C" int host_pa(void) { return g_last.a; }
extern "C" int host_pb(void) { return g_last.b; }
extern "C" int host_pc(void) { return g_last.c; }

// frame buffer: crashman writes its composed RGBA; the app reads + blits.
extern "C" void host_frame_write(const void* rgba, int w, int h) {
    HostShared* hh = host_shared(); if (!hh || !rgba) return;
    int64_t bytes = (int64_t)w * h * 4;
    if (bytes <= 0 || bytes > HM_FRAME_MAX) return;
    memcpy(hh->frame, rgba, (size_t)bytes);
    hh->fw = w; hh->fh = h;
    __sync_synchronize();
    hh->fseq = hh->fseq + 1;
    host_post(1, HM_FRAME, w, h, hh->fseq);
}
extern "C" int  host_frame_seq(void) { HostShared* h = host_shared(); return h ? h->fseq : -1; }
extern "C" int  host_frame_w(void)   { HostShared* h = host_shared(); return h ? h->fw : 0; }
extern "C" int  host_frame_h(void)   { HostShared* h = host_shared(); return h ? h->fh : 0; }
// copy the frame into `out` (out must hold w*h*4). returns seq copied.
extern "C" int  host_frame_read(void* out, int maxbytes) {
    HostShared* h = host_shared(); if (!h || !out) return -1;
    int64_t bytes = (int64_t)h->fw * h->fh * 4;
    if (bytes <= 0 || bytes > maxbytes) return -1;
    memcpy(out, h->frame, (size_t)bytes);
    return h->fseq;
}

// ---- legacy dictation mailbox (thin wrappers over the message ring so the
// existing app/crashman dictation code keeps working during the transition) --
extern "C" void host_mailbox(const char* base) { (void)base; }
static int g_dict_state = 0, g_dict_live = 0;
extern "C" void host_dictate_set(int v) { g_dict_state = v; host_post(0, HM_DICTATE, v, 0, 0); }
extern "C" int  host_dictate_get(void)  {
    while (host_poll(0)) { if (g_last.type == HM_DICTATE) g_dict_state = g_last.a; }
    return g_dict_state;
}
extern "C" void host_live_set(int v) { g_dict_live = v; host_post(1, HM_LIVE, v, 0, 0); }
extern "C" int  host_live_get(void)  {
    while (host_poll(1)) { if (g_last.type == HM_LIVE) g_dict_live = g_last.a; }
    return g_dict_live;
}

// tee all stdout/stderr to /tmp/<name>.log (fresh each run) AND the terminal,
// so a run's full output is always readable without pasting. one tee thread
// drains a pipe the standard streams are redirected into.
#include <pthread.h>
static int  g_log_real = -1;
static int  g_log_file = -1;
static int  g_log_done = 0;
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
extern "C" void host_log_setup(const char* name) {
    if (g_log_done || !name || !*name) return;
    g_log_done = 1;
    // the app name (from silver-host) wins over the root-element ident, so the
    // tee and the crash handler both land in /tmp/<app>.log
    const char* app = getenv("SILVER_APP");
    if (app && *app) name = app;
    char path[256];
    snprintf(path, sizeof(path), "/tmp/%s.log", name);
    g_log_file = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (g_log_file < 0) return;
    int pf[2];
    if (pipe(pf) != 0) { close(g_log_file); g_log_file = -1; return; }
    g_log_real = dup(STDOUT_FILENO);
    dup2(pf[1], STDOUT_FILENO);
    dup2(pf[1], STDERR_FILENO);
    close(pf[1]);
    setvbuf(stdout, 0, _IOLBF, 0);
    setvbuf(stderr, 0, _IOLBF, 0);
    pthread_t t;
    pthread_create(&t, 0, host_log_thread, (void*)(long)pf[0]);
    pthread_detach(t);
}
#else
// non-linux stubs: the feature is linux-only (dma-buf), but the symbols must
// exist so AppLink links. every call reports "no connection".
extern "C" int  appshare_listen(const char* p)                     { return -1; }
extern "C" int  host_ask_crashman(void)                            { return -1; }
extern "C" void host_dictate_set(int v)                            { }
extern "C" int  host_dictate_get(void)                             { return -1; }
extern "C" void host_live_set(int v)                               { }
extern "C" int  host_live_get(void)                                { return -1; }
extern "C" void host_log_setup(const char* name)                   { }
extern "C" void host_mailbox(const char* base)                     { }
extern "C" int  host_pid_alive(int pid)                            { return 0; }
extern "C" void host_post(int r, int t, int a, int b, int c)       { }
extern "C" int  host_poll(int r)                                   { return 0; }
extern "C" int  host_pa(void)                                      { return 0; }
extern "C" int  host_pb(void)                                      { return 0; }
extern "C" int  host_pc(void)                                      { return 0; }
extern "C" void host_frame_write(const void* p, int w, int h)      { }
extern "C" int  host_frame_seq(void)                               { return -1; }
extern "C" int  host_frame_w(void)                                 { return 0; }
extern "C" int  host_frame_h(void)                                 { return 0; }
extern "C" int  host_frame_read(void* o, int m)                    { return -1; }
extern "C" int  appshare_accept(int s)                             { return -1; }
extern "C" int  appshare_connect(const char* p)                    { return -1; }
extern "C" int  appshare_send(int s, const void* m)                { return -1; }
extern "C" int  appshare_send_fd(int s, int fd, const void* m)     { return -1; }
extern "C" int  appshare_recv_fd(int s, void* b, int* fdo)         { return -1; }
extern "C" int  appshare_send_msg(int s, const void* m, const void* p, int n) { return -1; }
extern "C" int  appshare_recv_payload(int s, void* b, int n)       { return -1; }
extern "C" void appshare_close(int fd)                             { }
#endif
