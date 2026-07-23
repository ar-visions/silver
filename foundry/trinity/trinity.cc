#include <cstdio>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#if defined(__linux__)
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

// is pid alive? distinguishes an app RELOAD (in-process, same pid) from an
// app EXIT (pid gone) so a consumer knows whether to keep running.
#include <signal.h>
#include <stdlib.h>
extern "C" int host_pid_alive(int pid) {
    if (pid <= 0) return 0;
    return kill(pid, 0) == 0 ? 1 : 0;
}

// ===========================================================================
// the app hosting channel. ONE anonymous memfd (RAM-backed, no filesystem
// name, no socket, no file — EVER) created by silver-host and inherited by
// every process it spawns (SILVER_SHM_FD). it holds a table of app slots;
// each slot is two SPSC message rings plus two shared-texture descriptors.
//
// pixels never cross the channel. every app already renders into its screen
// texture; when hosted, that texture is allocated as an exportable dma-buf
// and the descriptor below names its fd. a consumer duplicates the fd out of
// the publisher with pidfd_getfd and imports the SAME GPU memory as a
// VkImage. no copies, no broker, no per-frame traffic — just the texture.
// this struct layout MIRRORS silver-host.c — keep the two in lockstep.
// ===========================================================================
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/prctl.h>

enum {
    HM_NONE    = 0,
    HM_DICTATE = 1,   // app->ide     a=state (0 idle,1 rec,2 cancel)
    HM_LIVE    = 2,   // ide->app     a=0/1 mic live (button blue)
    HM_PRESS   = 3,   //              a=x b=y c=button
    HM_RELEASE = 4,   //              a=x b=y c=button
    HM_MOVE    = 5,   //              a=x b=y
    HM_KEY     = 6,   //              a=unicode b=state c=mods
    HM_TEXT    = 7,   //              a=codepoint
    HM_RESIZE  = 9,   //              a=w b=h (render at this size)
    HM_CLOSE   = 10   // either       overlay/pane closed
};

#define HM_SLOTS  1024
#define HOST_APPS 8
typedef struct { int32_t type, a, b, c; } HostMsg;
typedef struct { volatile uint32_t head, tail; HostMsg m[HM_SLOTS]; } HostRing;
// a cross-process shared Vulkan surface: TWO textures + a front index, so a
// consumer always samples a COMPLETE frame while the producer fills the
// other (single-texture sharing tears — the consumer catches the clear).
// fds are dma-buf fd NUMBERS valid in the publisher (pid); consumers
// duplicate them via pidfd_getfd. gen bumps on every (re)publish — a resize
// makes new textures, new fds, a new gen. front flips per finished frame.
typedef struct {
    volatile int32_t pid, fd0, fd1, front, gen, width, height, format;
} SharedTex;
typedef struct {
    HostRing  to_ide;      // toward the ide/consumer side
    HostRing  to_app;      // toward the app side (input, resize, close)
    SharedTex app_tex;     // the app's screen texture
    SharedTex ide_tex;     // orbiter's screen texture (summon overlay)
    volatile int32_t app_pid;   // process bound to this slot
    volatile int32_t state;     // 0 free, 1 spawn requested, 2 live, 3 exited
    char name[192];             // "module [default-arg]" silver-host spawns
} HostApp;
typedef struct {
    volatile int32_t host_pid;  // supervising silver-host; spawn asks signal it
    HostApp app[HOST_APPS];
} HostShared;

static HostShared* g_hs = 0;
static HostShared* host_shared(void) {
    if (g_hs) return g_hs;
    // NO file. silver-host created an anonymous memfd, sized it, and handed
    // us the fd by inheritance — its number is in SILVER_SHM_FD.
    const char* fds = getenv("SILVER_SHM_FD");
    if (!fds || !*fds) return 0;
    int fd = atoi(fds);
    if (fd < 0) return 0;
    void* p = mmap(0, sizeof(HostShared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) return 0;
    g_hs = (HostShared*)p;
    return g_hs;
}

// this process's own slot (SILVER_APP_SLOT, 0 when unset — the primary app)
extern "C" int host_slot(void) {
    const char* s = getenv("SILVER_APP_SLOT");
    return (s && *s) ? atoi(s) : 0;
}

// post a message to a slot's ring (0 = to_ide, 1 = to_app). SPSC.
extern "C" void host_post2(int slot, int ring, int type, int a, int b, int c) {
    HostShared* h = host_shared();
    if (!h || slot < 0 || slot >= HOST_APPS) return;
    HostRing* r = ring ? &h->app[slot].to_app : &h->app[slot].to_ide;
    uint32_t head = r->head;
    r->m[head % HM_SLOTS].type = type;
    r->m[head % HM_SLOTS].a = a;
    r->m[head % HM_SLOTS].b = b;
    r->m[head % HM_SLOTS].c = c;
    __sync_synchronize();
    r->head = head + 1;
}

// pop next message from a slot's ring; fills g_last, returns type (0 = empty).
static HostMsg g_last;
extern "C" int host_poll2(int slot, int ring) {
    HostShared* h = host_shared();
    if (!h || slot < 0 || slot >= HOST_APPS) return 0;
    HostRing* r = ring ? &h->app[slot].to_app : &h->app[slot].to_ide;
    if (r->tail == r->head) { g_last.type = 0; return 0; }
    g_last = r->m[r->tail % HM_SLOTS];
    __sync_synchronize();
    r->tail = r->tail + 1;
    return g_last.type;
}
extern "C" int host_pa(void) { return g_last.a; }
extern "C" int host_pb(void) { return g_last.b; }
extern "C" int host_pc(void) { return g_last.c; }

// own-slot conveniences (the common case for an app or a summoned orbiter)
extern "C" void host_post(int ring, int type, int a, int b, int c) {
    host_post2(host_slot(), ring, type, a, b, c);
}
extern "C" int host_poll(int ring) { return host_poll2(host_slot(), ring); }

// publish this process's shared surface (side 0 = app, 1 = ide): both
// texture fds at once. the fds stay open here for the textures' whole
// life; consumers pull dups.
extern "C" void host_tex_publish(int slot, int side, int fd0, int fd1, int w, int h, int fmt) {
    HostShared* hs = host_shared();
    if (!hs || slot < 0 || slot >= HOST_APPS) return;
    // let any same-uid consumer pull our fds (yama scope 1 blocks otherwise)
    prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
    SharedTex* t = side ? &hs->app[slot].ide_tex : &hs->app[slot].app_tex;
    t->pid    = (int)getpid();
    t->fd0    = fd0;
    t->fd1    = fd1;
    t->front  = -1;              // nothing complete yet
    t->width  = w;
    t->height = h;
    t->format = fmt;
    __sync_synchronize();
    t->gen = t->gen + 1;
    if (!side) hs->app[slot].state = 2;
}

extern "C" int host_tex_gen(int slot, int side) {
    HostShared* hs = host_shared();
    if (!hs || slot < 0 || slot >= HOST_APPS) return 0;
    SharedTex* t = side ? &hs->app[slot].ide_tex : &hs->app[slot].app_tex;
    return t->gen;
}

// producer: this texture now holds a COMPLETE frame — show it
extern "C" void host_tex_flip(int slot, int side, int front) {
    HostShared* hs = host_shared();
    if (!hs || slot < 0 || slot >= HOST_APPS) return;
    SharedTex* t = side ? &hs->app[slot].ide_tex : &hs->app[slot].app_tex;
    __sync_synchronize();
    t->front = front;
}

// consumer: which of the two textures holds the newest complete frame
extern "C" int host_tex_front(int slot, int side) {
    HostShared* hs = host_shared();
    if (!hs || slot < 0 || slot >= HOST_APPS) return -1;
    SharedTex* t = side ? &hs->app[slot].ide_tex : &hs->app[slot].app_tex;
    return t->front;
}

// duplicate one published texture fd (which = 0/1) out of its owner.
// returns a LOCAL fd (caller owns it; vulkan import consumes it) or -1.
extern "C" int host_tex_pull(int slot, int side, int which, int* w, int* h, int* fmt) {
    HostShared* hs = host_shared();
    if (!hs || slot < 0 || slot >= HOST_APPS) return -1;
    SharedTex* t = side ? &hs->app[slot].ide_tex : &hs->app[slot].app_tex;
    int fd = which ? t->fd1 : t->fd0;
    if (t->gen <= 0 || t->pid <= 0 || fd < 0) return -1;
    if (w)   *w   = t->width;
    if (h)   *h   = t->height;
    if (fmt) *fmt = t->format;
    if (t->pid == (int)getpid()) return dup(fd);
    int pfd = (int)syscall(SYS_pidfd_open, (pid_t)t->pid, 0);
    if (pfd < 0) return -1;
    int r = (int)syscall(SYS_pidfd_getfd, pfd, fd, 0);
    close(pfd);
    return r;
}

// ask silver-host to build (if needed) + spawn a module into a free slot.
// returns the slot to watch, or -1 (no supervisor / table full).
extern "C" int host_app_request(const char* nm) {
    HostShared* hs = host_shared();
    if (!hs || !nm || !*nm || hs->host_pid <= 0) return -1;
    for (int i = 1; i < HOST_APPS; i++) {
        HostApp* ap = &hs->app[i];
        if (ap->state == 0 || ap->state == 3) {
            memset((void*)&ap->app_tex, 0, sizeof(SharedTex));
            memset((void*)&ap->ide_tex, 0, sizeof(SharedTex));
            ap->to_ide.tail = ap->to_ide.head;
            ap->to_app.tail = ap->to_app.head;
            ap->app_pid = 0;
            strncpy((char*)ap->name, nm, sizeof(ap->name) - 1);
            ((char*)ap->name)[sizeof(ap->name) - 1] = 0;
            __sync_synchronize();
            ap->state = 1;
            kill((pid_t)hs->host_pid, SIGUSR1);
            return i;
        }
    }
    return -1;
}

extern "C" int host_app_state(int slot) {
    HostShared* hs = host_shared();
    if (!hs || slot < 0 || slot >= HOST_APPS) return 0;
    return hs->app[slot].state;
}

extern "C" int host_app_pid(int slot) {
    HostShared* hs = host_shared();
    if (!hs || slot < 0 || slot >= HOST_APPS) return 0;
    return hs->app[slot].app_pid;
}

// stop a hosted app; silver-host reaps it and marks the slot exited
extern "C" void host_app_stop(int slot) {
    HostShared* hs = host_shared();
    if (!hs || slot <= 0 || slot >= HOST_APPS) return;
    int pid = hs->app[slot].app_pid;
    if (pid > 0 && kill((pid_t)pid, 0) == 0) kill((pid_t)pid, SIGTERM);
}

// ask the supervising silver-host to bring up orbiter (SIGUSR1). guarded on
// the isolate-child marker so an unsupervised app never signals its ide.
extern "C" int host_ask_orbiter(void) {
    if (!getenv("SILVER_ISOLATE_CHILD")) return -1;
    HostShared* hs = host_shared();
    if (hs && hs->host_pid > 0) return kill((pid_t)hs->host_pid, SIGUSR1);
    return kill(getppid(), SIGUSR1);
}

// dictate (app->ide) and live-confirm (ide->app) are single latest-wins
// signals over the rings
extern "C" void host_dictate_set(int v) { host_post(0, HM_DICTATE, v, 0, 0); }
extern "C" void host_live_set(int v)    { host_post(1, HM_LIVE, v, 0, 0); }

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
    // hosted apps (spawned into a slot) APPEND: the supervisor already truncated the
    // log and wrote the build output into it — the console tails this file, so
    // truncating here would erase the compilation output. the primary app truncates.
    const char* slot = getenv("SILVER_APP_SLOT");
    int lflags = (slot && *slot) ? (O_WRONLY | O_CREAT | O_APPEND)
                                 : (O_WRONLY | O_CREAT | O_TRUNC);
    g_log_file = open(path, lflags, 0644);
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
// non-linux stubs: hosting is linux-only (dma-buf, memfd, pidfd), but the
// symbols must exist so the module links. every call reports "nothing there".
extern "C" int  host_ask_orbiter(void)                             { return -1; }
extern "C" void host_dictate_set(int v)                            { }
extern "C" void host_live_set(int v)                               { }
extern "C" void host_log_setup(const char* name)                   { }
extern "C" int  host_pid_alive(int pid)                            { return 0; }
extern "C" int  host_slot(void)                                    { return 0; }
extern "C" void host_post(int r, int t, int a, int b, int c)       { }
extern "C" int  host_poll(int r)                                   { return 0; }
extern "C" void host_post2(int s, int r, int t, int a, int b, int c) { }
extern "C" int  host_poll2(int s, int r)                           { return 0; }
extern "C" int  host_pa(void)                                      { return 0; }
extern "C" int  host_pb(void)                                      { return 0; }
extern "C" int  host_pc(void)                                      { return 0; }
extern "C" void host_tex_publish(int s, int sd, int f0, int f1, int w, int h, int f) { }
extern "C" int  host_tex_gen(int s, int sd)                        { return 0; }
extern "C" void host_tex_flip(int s, int sd, int fr)               { }
extern "C" int  host_tex_front(int s, int sd)                      { return -1; }
extern "C" int  host_tex_pull(int s, int sd, int wh, int* w, int* h, int* f) { return -1; }
extern "C" int  host_app_request(const char* nm)                   { return -1; }
extern "C" int  host_app_state(int s)                              { return 0; }
extern "C" int  host_app_pid(int s)                                { return 0; }
extern "C" void host_app_stop(int s)                               { }
#endif
