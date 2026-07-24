#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <signal.h>
#include <execinfo.h>
#include <sys/wait.h>
#include <spawn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <sys/syscall.h>

extern char** environ;

static void symbolize_crash_log(const char* appname);

// ---- app hosting channel -----------------------------------------------------
// ONE anonymous memfd (RAM-backed shared memory, no filesystem name, no
// socket, no file — EVER) created by this process and inherited by every
// child we spawn (SILVER_SHM_FD). it holds a table of app slots: two message
// rings + two shared-texture descriptors each. apps publish the dma-buf fd
// of the screen texture they already render into; consumers pull it with
// pidfd_getfd and import the same GPU memory. this process only spawns and
// reaps — it never touches pixels or fds. layout MIRRORS trinity.cc.
#define HM_SLOTS   1024
#define HOST_APPS  8
typedef struct { int32_t type, a, b, c; } HostMsg;
typedef struct { volatile uint32_t head, tail; HostMsg m[HM_SLOTS]; } HostRing;
typedef struct { volatile int32_t pid, fd0, fd1, front, gen, width, height, format; } SharedTex;
typedef struct {
    HostRing  to_ide, to_app;
    SharedTex app_tex, ide_tex;
    volatile int32_t app_pid;   // process bound to this slot
    volatile int32_t state;     // 0 free, 1 spawn requested, 2 live, 3 exited
    char name[192];             // "module [default-arg]" to spawn
} HostApp;
typedef struct {
    volatile int32_t host_pid;  // this supervisor; spawn requests SIGUSR1 it
    HostApp app[HOST_APPS];
} HostShared;

static int rebuild_blocking(const char* name, int clean);

typedef int        (*frame_fn)(void);
typedef void       (*destroy_fn)(void);
typedef void       (*init_fn)(void);
typedef int        (*au_compile_ready_fn)(void);
typedef void       (*au_compile_invoke_fn)(const char*);
typedef void       (*au_main_args_fn)(int, char**);
typedef void       (*au_live_set_pending_fn)(int);
typedef int        (*au_live_take_apply_fn)(void);
typedef int        (*au_live_get_defer_fn)(void);

// stash the process argv into libAu (loaded inside the app .so) so silver_live_init
// can parse the app's command-line flags into its instance. safe no-op on old libs.
static void stash_args(void* handle, int argc, char** argv) {
    au_main_args_fn set_args = (au_main_args_fn)dlsym(handle, "au_main_args");
    if (set_args) set_args(argc, argv);
}
#define FRAME_SYM   "silver_live_frame"
#define DESTROY_SYM "silver_live_destroy"
#define INIT_SYM    "silver_live_init"

#define MAX_SOURCES 128

static const char* g_app_name = "app";

// ---- process isolation (SILVER_ISOLATE=1) -----------------------------------
// normally the module is dlopen'd straight into this process: no IPC, no added
// latency, and gdb lands on the actual code. that is the right default.
//
// with SILVER_ISOLATE set we re-exec ourselves as a child that does the dlopen,
// and this process becomes a supervisor. the point is that a fault in the app
// no longer takes down whoever holds the window and the mic — the supervisor
// survives it, keeps the shared VkImage (its own dma-buf fd keeps the buffer
// alive after the child is gone), and can report what failed.
//
// the child is told not to recurse by SILVER_ISOLATE_CHILD. the parent then
// continues through main() as `orbiter` — the IDE shell that owns the
// window and reports what failed over the last frame the app published.
#define ISOLATE_ENV        "SILVER_ISOLATE"
#define ISOLATE_CHILD_ENV  "SILVER_ISOLATE_CHILD"
#define ISOLATE_APP_ENV    "SILVER_ISOLATE_APP"
#define ISOLATE_APP_PID_ENV "SILVER_ISOLATE_APP_PID"
// the SESSION key = this supervisor's pid. it names only the agent-context
// snapshot files (PNG/tree) the agent ingests — NOT the channel, which is an
// anonymous memfd inherited by fd, so N app copies never collide.
#define ISOLATE_SESSION_ENV "SILVER_ISOLATE_SESSION"
#define ISOLATE_STATUS_ENV "SILVER_ISOLATE_STATUS"
#define IDE_ENV            "IN_IDE"
#define ISOLATE_RESTART_ENV "SILVER_ISOLATE_RESTART"
#define ISOLATE_SHELL      "orbiter"

static pid_t g_isolate_child = 0;
static char  g_isolate_cwd[4096];
static int   g_argc;
static char** g_argv;

// the app<->ide channel: an anonymous memfd (RAM-backed shared memory, no
// filesystem name, no socket) created once here and inherited by every
// process we spawn. its number in each child is published as SILVER_SHM_FD.
static int         g_shm_fd = -1;
static HostShared* g_shm    = NULL;
#define SILVER_SHM_CHILD_FD 21   // fixed number the channel lands on in children

static void shm_create(void) {
    if (g_shm_fd >= 0) return;
    g_shm_fd = memfd_create("silver-ide", 0);
    if (g_shm_fd < 0) { perror("silver-host: memfd_create"); return; }
    if (ftruncate(g_shm_fd, sizeof(HostShared)) != 0) {
        perror("silver-host: ftruncate"); close(g_shm_fd); g_shm_fd = -1; return;
    }
    void* p = mmap(0, sizeof(HostShared), PROT_READ | PROT_WRITE, MAP_SHARED, g_shm_fd, 0);
    if (p == MAP_FAILED) { close(g_shm_fd); g_shm_fd = -1; return; }
    memset(p, 0, sizeof(HostShared));
    g_shm = (HostShared*)p;
    g_shm->host_pid = (int32_t)getpid();
}

// place the channel memfd at the fixed child fd number and name it in env.
// call inside a spawn's file-actions + env build.
static void shm_inherit(posix_spawn_file_actions_t* fa, char* fdenv, size_t cap) {
    if (g_shm_fd >= 0)
        posix_spawn_file_actions_adddup2(fa, g_shm_fd, SILVER_SHM_CHILD_FD);
    snprintf(fdenv, cap, "SILVER_SHM_FD=%d", g_shm_fd >= 0 ? SILVER_SHM_CHILD_FD : -1);
}

// isolation is the DEFAULT: the window and mic survive an app fault. off when:
// we ARE the child; the app is hosted via --attach (an orbiter pane IS the
// isolation); IN_IDE is set (in-process keeps gdb on real code); or
// SILVER_ISOLATE=0. an explicit SILVER_ISOLATE=1 overrides IN_IDE.
static int isolate_requested(int argc, char** argv) {
    if (getenv(ISOLATE_CHILD_ENV)) return 0;      // we ARE the child
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--attach") == 0) return 0;
    const char* v = getenv(ISOLATE_ENV);
    if (v && *v) return strcmp(v, "0") != 0;
    if (getenv(IDE_ENV)) return 0;
    return 1;
}

// spawn the app child: same binary, same argv. runs from the LAUNCH cwd (a
// relaunch happens after cd_share moved us to orbiter's share dir).
// posix_spawn, NOT fork(): a relaunch happens with Vulkan fully up, and
// fork() from a live nvidia/UVM process poisons the PARENT — its next queue
// submit blocks forever in the driver. (verified: relaunch wedged the frame
// loop inside Command_submit until fork was removed.)
static pid_t isolate_spawn(void) {
    char** cargv = malloc((g_argc + 1) * sizeof(char*));
    for (int i = 0; i < g_argc; i++) cargv[i] = g_argv[i];
    cargv[g_argc] = NULL;
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    if (g_isolate_cwd[0])  posix_spawn_file_actions_addchdir_np(&fa, g_isolate_cwd);
    static char fdenv[32];
    shm_inherit(&fa, fdenv, sizeof(fdenv));   // hand the app the channel memfd
    int n = 0;
    while (environ[n]) n++;
    char** cenv = malloc((n + 3) * sizeof(char*));
    memcpy(cenv, environ, n * sizeof(char*));
    cenv[n]     = ISOLATE_CHILD_ENV "=1";
    cenv[n + 1] = fdenv;
    cenv[n + 2] = NULL;
    pid_t pid = 0;
    int sp = posix_spawn(&pid, "/proc/self/exe", &fa, NULL, cargv, cenv);
    posix_spawn_file_actions_destroy(&fa);
    free(cargv);
    free(cenv);
    if (sp != 0) {
        fprintf(stderr, "silver-host: isolate spawn failed: %s\n", strerror(sp));
        return -1;
    }
    return pid;
}

// a hosted-app spawn request (orbiter wrote name + state=1 into a slot and
// signalled us). build the module if its binary is missing, then spawn it
// attached: it inherits the channel memfd and publishes its screen texture
// into its slot. the binary is itself this supervisor for that module, so a
// stale build recompiles on its own at startup.
static void spawn_slot_app(int k, const char* bindir) {
    HostApp* ap = &g_shm->app[k];
    char name[192];
    strncpy(name, (const char*)ap->name, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    if (!name[0]) { ap->state = 3; return; }
    // "module [default-arg]": the arg (a document path) rides the spawn argv
    char* arg = strchr(name, ' ');
    if (arg) { *arg = 0; arg++; }
    // leading markers: '!' = debug (the app self-stops before init so orbiter
    // can attach lldb); '*' = clean (full --clean rebuild before the spawn).
    int   dbg = 0, clean = 0;
    char* nm  = name;
    while (*nm == '!' || *nm == '*') {
        if (*nm == '!') dbg   = 1;
        if (*nm == '*') clean = 1;
        nm++;
    }
    // app binaries live beside this supervisor binary (one products dir)
    char bin[4300];
    snprintf(bin, sizeof(bin), "%s/%s", bindir, nm);
    // fresh app log for this run; the app APPENDS (host_log_setup honors the slot),
    // so the build output below + the app's runtime both land here for the console.
    { char lp[256]; snprintf(lp, sizeof(lp), "/tmp/%s.log", nm);
      int lfd = open(lp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (lfd >= 0) close(lfd); }
    struct stat st;
    if ((clean || stat(bin, &st) != 0) && rebuild_blocking(nm, clean) != 0) {
        fprintf(stderr, "silver-host: %s build failed — slot %d dead\n", name, k);
        ap->state = 3;
        return;
    }
    char* cargv[] = { bin, "--attach", "shm", arg, NULL };
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    if (g_isolate_cwd[0]) posix_spawn_file_actions_addchdir_np(&fa, g_isolate_cwd);
    static char fdenv[32];
    shm_inherit(&fa, fdenv, sizeof(fdenv));
    static char slotenv[32];
    snprintf(slotenv, sizeof(slotenv), "SILVER_APP_SLOT=%d", k);
    int n = 0;
    while (environ[n]) n++;
    char** cenv = malloc((n + 4) * sizeof(char*));
    memcpy(cenv, environ, n * sizeof(char*));
    cenv[n]     = fdenv;
    cenv[n + 1] = slotenv;
    cenv[n + 2] = dbg ? (char*)"SILVER_DEBUG=1" : NULL;
    cenv[n + 3] = NULL;
    pid_t pid = 0;
    int rc = posix_spawn(&pid, bin, &fa, NULL, cargv, cenv);
    posix_spawn_file_actions_destroy(&fa);
    free(cenv);
    if (rc != 0) {
        fprintf(stderr, "silver-host: spawn %s failed: %s\n", name, strerror(rc));
        ap->state = 3;
        return;
    }
    ap->app_pid = (int32_t)pid;
    ap->state   = 2;
    fprintf(stderr, "silver-host: hosting %s (pid %d, slot %d)\n", name, (int)pid, k);
}

// kill every hosted slot app (the window that hosts them is going away)
static void slots_shutdown(void) {
    if (!g_shm) return;
    for (int k = 1; k < HOST_APPS; k++) {
        int p = g_shm->app[k].app_pid;
        if (g_shm->app[k].state == 2 && p > 0 && kill((pid_t)p, 0) == 0)
            kill((pid_t)p, SIGTERM);
    }
}

// the app asks for orbiter by signaling its supervisor with SIGUSR1
static volatile sig_atomic_t g_ask_orbiter = 0;
static void on_sigusr1(int sig) { (void)sig; g_ask_orbiter = 1; }
// the attached orbiter's pid, so a reload can't summon a SECOND one — the
// app hot-reloads (claude editing it), re-asks, and we must reuse the shell
// that is already up rather than orphaning it.
static pid_t g_shell_pid = 0;

// phase 1: THIN supervision. the child owns a normal window; this process
// holds nothing (no vulkan, no orbiter) and just waits — the usual lifecycle
// is one app process with an invisible supervisor. orbiter loads only when
// the child crashes or the app asks (SIGUSR1). returns 1 = continue as
// orbiter, 0 = child exited normally (*exit_code set), -1 = fall back
// in-process (spawn failed).
static int supervise_wait(int argc, char** argv, const char* appname,
                          const char* bindir, int* exit_code) {
    g_argc = argc;
    g_argv = argv;
    if (!getcwd(g_isolate_cwd, sizeof(g_isolate_cwd))) g_isolate_cwd[0] = '\0';
    // session key still names the agent-context snapshots (real PNG/tree
    // files the agent ingests) — NOT the channel, which is anonymous.
    char sess[64];
    snprintf(sess, sizeof(sess), "%d", (int)getpid());
    setenv(ISOLATE_SESSION_ENV, sess, 1);
    // the anonymous channel exists BEFORE any child, so all inherit the same
    // memfd. no file, no socket, no session-keyed path.
    shm_create();
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigusr1;          // no SA_RESTART: waitpid must EINTR
    sigaction(SIGUSR1, &sa, NULL);
    pid_t pid = isolate_spawn();
    if (pid < 0) return -1;
    printf("silver-host: supervising %s (pid %d) — %s loads on crash or ask\n",
        appname, pid, ISOLATE_SHELL);
    for (;;) {
        int   st = 0;
        pid_t r  = waitpid(-1, &st, 0);   // -1: also reaps hosted apps + shell
        if (r < 0 && errno == EINTR) {
            if (!g_ask_orbiter) continue;
            g_ask_orbiter = 0;
            // hosted-app spawn requests first: a child wrote name + state=1
            // into slots and signalled us. if any exist, this signal was a
            // spawn ask, not a summon.
            int spawned = 0;
            if (g_shm)
                for (int k = 1; k < HOST_APPS; k++)
                    if (g_shm->app[k].state == 1) { spawn_slot_app(k, bindir); spawned = 1; }
            if (spawned) continue;
            // a shell is already attached (the app hot-reloaded and re-asked)
            // — reuse it, never spawn a duplicate. kill(pid,0) probes liveness.
            if (g_shell_pid > 0 && kill(g_shell_pid, 0) == 0) {
                fprintf(stderr, "silver-host: orbiter already up (pid %d) — reusing\n",
                    (int)g_shell_pid);
                continue;
            }
            // the app asked: orbiter INSIDE the app. the app's screen texture
            // is published in slot 0; orbiter pulls the fd itself — nothing
            // to broker, nothing to inherit but the channel memfd.
            char shell[4200];
            snprintf(shell, sizeof(shell), "%s/%s", bindir, ISOLATE_SHELL);
            char* sargv[] = { shell, "--attach", "shm", NULL };
            posix_spawn_file_actions_t fa;
            posix_spawn_file_actions_init(&fa);
            static char fdenv[32];
            shm_inherit(&fa, fdenv, sizeof(fdenv));
            int n = 0;
            while (environ[n]) n++;
            char** cenv = malloc((n + 5) * sizeof(char*));
            memcpy(cenv, environ, n * sizeof(char*));
            static char appenv[256];
            snprintf(appenv, sizeof(appenv), ISOLATE_APP_ENV "=%s", appname);
            static char pidenv[64];
            snprintf(pidenv, sizeof(pidenv), ISOLATE_APP_PID_ENV "=%d", (int)pid);
            static char slotenv[32];
            snprintf(slotenv, sizeof(slotenv), "SILVER_APP_SLOT=%d", 0);
            cenv[n]     = appenv;
            cenv[n + 1] = pidenv;
            cenv[n + 2] = slotenv;
            cenv[n + 3] = fdenv;
            cenv[n + 4] = NULL;
            pid_t sp = 0;
            int rc = posix_spawn(&sp, shell, &fa, NULL, sargv, cenv);
            posix_spawn_file_actions_destroy(&fa);
            free(cenv);
            if (rc != 0)
                fprintf(stderr, "silver-host: orbiter spawn failed: %s\n", strerror(rc));
            else {
                g_shell_pid = sp;
                fprintf(stderr, "silver-host: orbiter attached inside %s (pid %d)\n",
                    appname, (int)sp);
            }
            continue;
        }
        if (r < 0) {
            perror("silver-host: waitpid");
            *exit_code = 1;
            return 0;
        }
        if (r != pid) {                    // a hosted app or the shell exited
            if (r == g_shell_pid) g_shell_pid = 0;
            if (g_shm)
                for (int k = 1; k < HOST_APPS; k++)
                    if (g_shm->app[k].app_pid == (int32_t)r) g_shm->app[k].state = 3;
            continue;
        }
        if (WIFSIGNALED(st)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "signal %d (%s)",
                WTERMSIG(st), strsignal(WTERMSIG(st)));
            fprintf(stderr, "silver-host: app died: %s — loading %s\n",
                msg, ISOLATE_SHELL);
            symbolize_crash_log(appname);
            slots_shutdown();
            setenv(ISOLATE_APP_ENV, appname, 1);
            setenv(ISOLATE_STATUS_ENV, msg, 1);
            // continue as orbiter IN-PROCESS: point the channel env at the
            // memfd we still hold (no inheritance step for in-process)
            if (g_shm_fd >= 0) {
                char fb[16]; snprintf(fb, sizeof(fb), "%d", g_shm_fd);
                setenv("SILVER_SHM_FD", fb, 1);
            }
            g_isolate_child = 0;
            return 1;
        }
        *exit_code = WIFEXITED(st) ? WEXITSTATUS(st) : 1;
        // app done — take the shell and every hosted app down with us so
        // nothing orphans against a dead channel
        if (g_shell_pid > 0 && kill(g_shell_pid, 0) == 0) kill(g_shell_pid, SIGTERM);
        slots_shutdown();
        return 0;
    }
}

// orbiter asks for a relaunch by setting SILVER_ISOLATE_RESTART (env is
// process-global — same channel as the status verdict, other direction).
// the app comes back WINDOWED (its own window, the usual mode) — orbiter
// keeps its window and keeps reaping, so a re-crash posts a fresh verdict.
static void isolate_restart_check(void) {
    if (g_isolate_child) return;
    const char* r = getenv(ISOLATE_RESTART_ENV);
    if (!r || !*r) return;
    unsetenv(ISOLATE_RESTART_ENV);
    unsetenv(ISOLATE_STATUS_ENV);
    pid_t pid = isolate_spawn();
    if (pid > 0) {
        g_isolate_child = pid;
        fprintf(stderr, "silver-host: relaunched app as pid %d\n", pid);
    }
}

// reap a dead isolated child without blocking; publish what happened through
// the environment (same process — orbiter reads it with getenv each frame).
static void isolate_reap(void) {
    if (!g_isolate_child) return;
    int   st = 0;
    pid_t r  = waitpid(g_isolate_child, &st, WNOHANG);
    if (r != g_isolate_child) {
        if (r < 0) g_isolate_child = 0;
        return;
    }
    g_isolate_child = 0;
    char msg[128];
    if (WIFSIGNALED(st))
        snprintf(msg, sizeof(msg), "signal %d (%s)",
            WTERMSIG(st), strsignal(WTERMSIG(st)));
    else
        snprintf(msg, sizeof(msg), "exit %d", WEXITSTATUS(st));
    fprintf(stderr, "silver-host: isolated app died: %s\n", msg);
    setenv(ISOLATE_STATUS_ENV, msg, 1);
    if (WIFSIGNALED(st)) symbolize_crash_log(getenv(ISOLATE_APP_ENV));
}

static void crash_handler(int sig) {
    void* frames[64];
    int   nframes = backtrace(frames, 64);
    const char* sname =
        sig == SIGSEGV ? "SIGSEGV" :
        sig == SIGABRT ? "SIGABRT" :
        sig == SIGBUS  ? "SIGBUS"  :
        sig == SIGILL  ? "SIGILL"  :
        sig == SIGFPE  ? "SIGFPE"  :
        sig == SIGTRAP ? "SIGTRAP" : "?";
    char hdr[256];
    int  hl = snprintf(hdr, sizeof(hdr), "\n%s: signal %d (%s)\n",
        g_app_name, sig, sname);
    if (hl < 0) hl = 0;
    // write the callstack STRAIGHT to /tmp/<app>.log, not via stderr — stderr
    // is dup'd into a pipe drained by a background thread that never runs once
    // we re-raise. backtrace_symbols_fd is async-signal-safe (no malloc), so it
    // survives even a heap-corruption crash. append: the tee already truncated.
    char lp[256];
    snprintf(lp, sizeof(lp), "/tmp/%s.log", g_app_name);
    int lf = open(lp, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (lf >= 0) {
        (void)write(lf, hdr, (size_t)hl);
        // module-relative addresses for the supervisor's addr2line
        // pass (return addrs -1 so lines point AT the call site)
        for (int i = 0; i < nframes; i++) {
            Dl_info di;
            if (dladdr(frames[i], &di) && di.dli_fname && di.dli_fbase) {
                unsigned long off =
                    (unsigned long)((char*)frames[i] - (char*)di.dli_fbase);
                if (i > 0 && off > 0) off -= 1;
                char fl[600];
                int  fn = snprintf(fl, sizeof(fl), "  @ %s +0x%lx\n",
                    di.dli_fname, off);
                if (fn > 0) (void)write(lf, fl, (size_t)fn);
            }
        }
        fsync(lf);
        close(lf);
    }
    (void)write(STDERR_FILENO, hdr, (size_t)hl);
    signal(sig, SIG_DFL);
    raise(sig);
}

// resolve the "  @ module +0xoff" frames the crash handler left in
// /tmp/<app>.log to file:line (addr2line reads the module's DWARF);
// runs in the supervisor AFTER the child died — popen is safe here
static void symbolize_crash_log(const char* appname) {
    if (!appname || !*appname) return;
    char lp[256];
    snprintf(lp, sizeof(lp), "/tmp/%s.log", appname);
    FILE* f = fopen(lp, "r");
    if (!f) return;
    char          line[1024];
    static char   mods[64][512];
    unsigned long offs[64];
    int           n = 0;
    while (fgets(line, sizeof(line), f)) {
        // a new crash header restarts collection: keep the LAST block
        if (strstr(line, ": signal ")) n = 0;
        char          m[512];
        unsigned long o;
        if (sscanf(line, "  @ %511s +0x%lx", m, &o) == 2 && n < 64) {
            snprintf(mods[n], sizeof(mods[n]), "%s", m);
            offs[n] = o;
            n++;
        }
    }
    fclose(f);
    if (!n) return;
    FILE* out = fopen(lp, "a");
    if (out) fprintf(out, "\nsymbolized:\n");
    fprintf(stderr, "silver-host: symbolized crash:\n");
    for (int i = 0; i < n; i++) {
        char cmd[700];
        snprintf(cmd, sizeof(cmd),
            "addr2line -e '%s' -f -C -p 0x%lx 2>/dev/null", mods[i], offs[i]);
        char  res[1024] = { 0 };
        FILE* p = popen(cmd, "r");
        if (p) {
            if (!fgets(res, sizeof(res), p)) res[0] = 0;
            pclose(p);
        }
        res[strcspn(res, "\n")] = 0;
        const char* base = strrchr(mods[i], '/');
        base = base ? base + 1 : mods[i];
        if (res[0]) {
            fprintf(stderr, "  %s\n", res);
            if (out) fprintf(out, "  %s\n", res);
        } else {
            fprintf(stderr, "  %s +0x%lx\n", base, offs[i]);
            if (out) fprintf(out, "  %s +0x%lx\n", base, offs[i]);
        }
    }
    if (out) fclose(out);
}

// NON-fatal backtrace on SIGUSR2: `kill -USR2 <pid>` dumps where the process
// is right now (to find a userspace spin) and keeps running.
static void probe_handler(int sig) {
    (void)sig;
    void*  frames[64];
    int    nframes = backtrace(frames, 64);
    char** syms    = backtrace_symbols(frames, nframes);
    fprintf(stderr, "\n%s: PROBE backtrace\n", g_app_name);
    for (int i = 0; i < nframes; i++)
        fprintf(stderr, "  #%d %s\n", i, syms ? syms[i] : "??");
    free(syms);
}

static time_t file_mtime(const char* path) {
    struct stat s;
    return (stat(path, &s) == 0) ? s.st_mtime : 0;
}

static void cd_share(const char* bindir, const char* name) {
    char share[4096];
    snprintf(share, sizeof(share), "%s/../share/%s", bindir, name);
    struct stat st;
    if (stat(share, &st) == 0 && S_ISDIR(st.st_mode))
        chdir(share);
}

static void* try_dlopen(const char* lib) {
    void* h = NULL;
#if defined(__SANITIZE_ADDRESS__) || (defined(__has_feature) && __has_feature(address_sanitizer)) || !defined(RTLD_DEEPBIND)
    // RTLD_DEEPBIND is a glibc extension — not available on macOS
    int flags = RTLD_NOW | RTLD_GLOBAL;
#else
    int flags = RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND;
#endif
    for (int i = 0; i < 20 && !h; i++) {
        h = dlopen(lib, flags);
        if (!h) usleep(50000);  // 50ms retry — handles linker write race
    }
    return h;
}

// dlopen caches by dev+inode. If the linker overwrites the .so in-place (same
// inode), dlopen returns the old cached handle. Copy to a unique /tmp path to
// guarantee a fresh inode on every hot-reload.
static void* reload_dlopen(const char* lib, time_t ts) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "/tmp/hotreload_%ld.so", (long)ts);
    FILE *src = fopen(lib, "rb");
    FILE *dst = fopen(tmp, "wb");
    if (src && dst) {
        char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
            fwrite(buf, 1, n, dst);
        fclose(src);
        fclose(dst);
        void* h = try_dlopen(tmp);
        unlink(tmp);  // unlink immediately — dlopen holds the inode open
        return h;
    }
    if (src) fclose(src);
    if (dst) { fclose(dst); unlink(tmp); }
    return try_dlopen(lib);  // fallback
}

typedef struct { char path[4096]; time_t mtime; } source_watch;

static int load_sources(const char* artifacts_path,
                        source_watch* srcs, int* nsr) {
    *nsr = 0;
    FILE *f = fopen(artifacts_path, "r");
    if (!f) return -1;
    char buf[4096];
    while (fgets(buf, sizeof(buf), f) && *nsr < MAX_SOURCES) {
        buf[strcspn(buf, "\n")] = '\0';
        if (!*buf) continue;
        strncpy(srcs[*nsr].path, buf, sizeof(srcs[*nsr].path) - 1);
        srcs[*nsr].mtime = file_mtime(buf);
        (*nsr)++;
    }
    fclose(f);
    return 0;
}

// true if the built product is missing or any watched source/artifact is newer
// than it — i.e. the loaded module would be stale.
static int sources_newer(const char* product, source_watch* srcs, int nsr) {
    time_t prod = file_mtime(product);       // follows the symlink to the .so
    if (!prod) return 1;                      // no product -> must build
    for (int i = 0; i < nsr; i++) {
        if (file_mtime(srcs[i].path) > prod) return 1;
    }
    return 0;
}

// recompile synchronously (no '&') so the fresh product is in place before load.
// run from SILVER_ROOT so silver resolves the module via foundry/<name>/ rather
// than relative to wherever the host was launched / cd_share'd to.
// returns 0 on a clean build, non-zero if silver failed or crashed. callers must
// NOT run the (stale) product when this fails.
// spawn the recompile WITHOUT waiting — the frame loop keeps the app live while
// silver builds; the caller reaps with waitpid(WNOHANG) and reloads on success.
static pid_t rebuild_spawn(const char* name, int clean) {
    char cmd[8192];
    // --build: compile ONLY. bare `silver <app>` would LAUNCH the app (silver_live_run execs
    // the live host), spawning a whole second process+window on every reload while this one
    // keeps running. we just want the fresh .so produced so the host below hot-swaps it.
    snprintf(cmd, sizeof(cmd),
        "cd \"" SILVER_ROOT "\" && \"" SILVER_ROOT "/install/bin/silver\" %s --build%s",
        name, clean ? " --clean" : "");
    // send the compile output to the app's OWN log so orbiter's console (which tails
    // /tmp/<app>.log) shows the compilation. spawn_slot_app truncated it beforehand,
    // and the app appends (host_log_setup) so build + runtime share the one file.
    char lp[256];
    snprintf(lp, sizeof(lp), "/tmp/%s.log", name);
    { int hfd = open(lp, O_WRONLY | O_CREAT | O_APPEND, 0644);
      if (hfd >= 0) {
          char h[300];
          int hl = snprintf(h, sizeof(h), "%s: compiling...\n", name);
          if (hl > 0) (void)write(hfd, h, (size_t)hl);
          close(hfd);
      } }
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, 1, lp, O_WRONLY | O_CREAT | O_APPEND, 0644);
    posix_spawn_file_actions_adddup2(&fa, 1, 2);
    // posix_spawn, NOT system(): system() fork()s, and this host is a multithreaded
    // GUI process (MoltenVK / libdispatch / GLFW). fork() from a multithreaded process
    // copies only the calling thread but inherits locks held by the others — the child
    // can deadlock on a malloc lock before it reaches exec, freezing the whole app.
    // posix_spawn execs without running user code in the child, so it can't deadlock.
    char* sh_argv[] = { "/bin/sh", "-c", cmd, NULL };
    pid_t pid = 0;
    int sp = posix_spawn(&pid, "/bin/sh", &fa, NULL, sh_argv, environ);
    posix_spawn_file_actions_destroy(&fa);
    if (sp != 0) {
        fprintf(stderr, "%s: BUILD ERROR — posix_spawn failed: %s\n", name, strerror(sp));
        return -1;
    }
    return pid;
}

// interpret a reaped compile child's status: 0 = good build
static int rebuild_status(int rc, const char* name) {
    if (WIFSIGNALED(rc)) {
        fprintf(stderr, "%s: BUILD CRASHED — silver died with signal %d (%s)\n",
            name, WTERMSIG(rc), strsignal(WTERMSIG(rc)));
        return -1;
    }
    if (WIFEXITED(rc) && WEXITSTATUS(rc) != 0) {
        fprintf(stderr, "%s: BUILD FAILED — silver exited %d\n", name, WEXITSTATUS(rc));
        return -1;
    }
    return 0;
}

static int rebuild_blocking(const char* name, int clean) {
    pid_t pid = rebuild_spawn(name, clean);
    if (pid < 0) return -1;
    int rc = 0;
    if (waitpid(pid, &rc, 0) < 0) {
        fprintf(stderr, "%s: BUILD ERROR — waitpid failed\n", name);
        return -1;
    }
    return rebuild_status(rc, name);
}

// recompile via the in-process compiler if the app published one, else shell out to silver.
static int do_recompile(void* handle, const char* name) {
    au_compile_ready_fn  ready_fn  = (au_compile_ready_fn) dlsym(handle,  "au_compile_ready");
    au_compile_invoke_fn invoke_fn = (au_compile_invoke_fn)dlsym(handle, "au_compile_invoke");
    if (ready_fn && ready_fn() && invoke_fn) {
        invoke_fn(name);   // in-process compiler reports its own errors
        return 0;
    }
    return rebuild_blocking(name, 0);
}

int main(int argc, char** argv) {
    printf("silver-host main\n");

    // resolve the running binary to an absolute path so product/source paths
    // survive the cd_share() that changes cwd. /proc/self/exe is the ACTUAL
    // exe regardless of how it was invoked — a bare name found on PATH (e.g.
    // `experiment`) makes realpath(argv[0]) fail and bindir collapse to ".".
    // fall back to realpath(argv[0]) only if /proc is unavailable.
    char abspath[4096];
    ssize_t exe_n = readlink("/proc/self/exe", abspath, sizeof(abspath) - 1);
    if (exe_n > 0) {
        abspath[exe_n] = '\0';
    } else if (!realpath(argv[0], abspath)) {
        strncpy(abspath, argv[0], sizeof(abspath) - 1);
        abspath[sizeof(abspath) - 1] = '\0';
    }
    char self[4096], self2[4096];
    strncpy(self,  abspath, sizeof(self)  - 1); self[sizeof(self)   - 1] = '\0';
    strncpy(self2, abspath, sizeof(self2) - 1); self2[sizeof(self2) - 1] = '\0';
    const char* name   = basename(self);
    const char* bindir = dirname(self2);

    // BEFORE anything that changes cwd or rebuilds: the child re-runs main from
    // the launch directory and does the whole normal startup itself. hooking in
    // later handed it the share dir as its launch cwd and made both processes
    // race the same rebuild. the supervisor blocks in supervise_wait for the
    // app's whole normal lifetime; it also spawns hosted apps into channel
    // slots on request. only a crash (or SIGUSR1 ask) continues this process
    // below AS the orbiter shell — same bindir, name swapped. orbiter itself
    // is supervised like any app — its silver-host hosts its pane apps.
    if (isolate_requested(argc, argv)) {
        int exit_code = 0;
        int r = supervise_wait(argc, argv, name, bindir, &exit_code);
        if (r == 0) return exit_code;
        if (r > 0) {
            // orbiter IS the crash shell — it must not relaunch itself
            if (strcmp(name, ISOLATE_SHELL) == 0) return exit_code ? exit_code : 1;
            name = ISOLATE_SHELL;
        }
        // r < 0: isolation unavailable — run the app in-process below
    } else if (strcmp(name, ISOLATE_SHELL) == 0) {
        // an unsupervised orbiter start is just the IDE: no inherited agent
        // contract (launching from inside a supervised app's console leaks
        // the env) — only --attach shells keep it
        int attached = 0;
        for (int i = 1; i < argc; i++)
            if (strcmp(argv[i], "--attach") == 0) attached = 1;
        if (!attached && !getenv(ISOLATE_CHILD_ENV)) {
            unsetenv(ISOLATE_APP_ENV);
            unsetenv(ISOLATE_APP_PID_ENV);
            unsetenv(ISOLATE_STATUS_ENV);
            unsetenv(ISOLATE_SESSION_ENV);
        }
    }
    g_app_name = name;
    // the log is named for the APP, not the root element — so the tee
    // (host_log_setup) and the crash handler agree on /tmp/<app>.log
    setenv("SILVER_APP", name, 1);

#ifdef SILVER_ROOT
    setenv("LD_LIBRARY_PATH",
        SILVER_ROOT "/install/lib:"
        SILVER_ROOT "/install/build",
        1);
#endif

    // the handler must run on an ALTERNATE stack: a stack overflow leaves no
    // room to deliver the signal on the faulting stack, so without SA_ONSTACK
    // the process dies silently (bash prints "Segmentation fault", the handler
    // never enters, no backtrace).
    {
        static char       altstack[64 * 1024];
        stack_t           ss = { .ss_sp = altstack, .ss_size = sizeof(altstack), .ss_flags = 0 };
        struct sigaction  sa;
        sigaltstack(&ss, NULL);
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = crash_handler;
        sa.sa_flags   = SA_ONSTACK;
        sigaction(SIGSEGV, &sa, NULL);
        sigaction(SIGABRT, &sa, NULL);
        sigaction(SIGBUS,  &sa, NULL);
        sigaction(SIGILL,  &sa, NULL);
        sigaction(SIGTRAP, &sa, NULL);   // compiler-emitted fault/bounds trap
        sigaction(SIGFPE,  &sa, NULL);
        // a closed shell/attach socket must surface as EPIPE, never kill us
        signal(SIGPIPE, SIG_IGN);
        // SIGUSR2: non-fatal backtrace probe (find a userspace spin)
        struct sigaction pa;
        memset(&pa, 0, sizeof(pa));
        pa.sa_handler = probe_handler;
        pa.sa_flags   = SA_ONSTACK | SA_RESTART;
        sigaction(SIGUSR2, &pa, NULL);
    }

    // --defer-reload (or SILVER_DEFER_RELOAD): don't auto-recompile on a source change —
    // stage it, signal the app it's pending, and only recompile + hot-swap when the app
    // requests it (au_live_request_apply). lets orbiter show a "reload ready" affordance.
    // deferred live-reload is normally turned ON BY THE APP at runtime (it calls
    // au_live_set_defer; the watch loop below polls au_live_get_defer each iteration). the
    // user passes nothing. SILVER_DEFER_RELOAD is just a dev override that forces it on.
    int host_defer = (getenv("SILVER_DEFER_RELOAD") != NULL);

    // SILVER_NO_RELOAD: live reload fully OFF — the watch loop never polls
    // sources and never recompiles; the app runs the build it started with
    int no_reload = (getenv("SILVER_NO_RELOAD") != NULL);

    char product[4096];
    snprintf(product, sizeof(product), "%s/%s.product", bindir, name);

    char artifacts[4096];
    snprintf(artifacts, sizeof(artifacts), "%s/%s.source", bindir, name);

    // record the launch cwd before we cd to the share, so the app can resolve its
    // config (e.g. orbiter.agi) against where it was started, not the share dir.
    char launch_cwd[4096];
    if (getcwd(launch_cwd, sizeof(launch_cwd))) {
        setenv("SILVER_STARTUP", launch_cwd, 1);
        printf("silver-host: launch cwd = %s (SILVER_STARTUP set)\n", launch_cwd);
    }
#ifdef SILVER_ROOT
    // apps resolve {SILVER}/export and foundry modules through this
    setenv("SILVER", SILVER_ROOT, 1);
#endif
    cd_share(bindir, name);

    // up-front staleness check: only recompile when a source is actually newer
    // than the built product. when nothing changed we skip the compile entirely
    // and dlopen the existing .so — no rebuild on every launch.
    source_watch srcs[MAX_SOURCES];
    int nsr = 0;
    load_sources(artifacts, srcs, &nsr);
    if (sources_newer(product, srcs, nsr)) {
        if (rebuild_blocking(name, 0) != 0) {
            fprintf(stderr, "%s: fix the build errors above and relaunch.\n", name);
            return 1;
        }
        load_sources(artifacts, srcs, &nsr);   // artifact list may have changed
    }

    char lib[4096];
    ssize_t n = readlink(product, lib, sizeof(lib) - 1);
    if (n < 0) {
        fprintf(stderr, "%s: no product file at %s\n", name, product);
        return 1;
    }
    lib[n] = '\0';

    void* handle = try_dlopen(lib);
    if (!handle) { fprintf(stderr, "%s: dlopen %s: %s\n", name, lib, dlerror()); return 1; }

    // initial startup: call silver_live_init explicitly (not a global constructor)
    init_fn    do_init    = dlsym(handle, INIT_SYM);
    frame_fn   do_frame   = dlsym(handle, FRAME_SYM);
    destroy_fn do_destroy = dlsym(handle, DESTROY_SYM);
    stash_args(handle, argc, argv);
    // debug launch: park HERE, before any app code, so orbiter can attach lldb and
    // arm breakpoints (including in init) before continuing us (SIGCONT).
    if (getenv("SILVER_DEBUG")) {
        fprintf(stderr, "silver-host: SILVER_DEBUG — pid %d stopped before init for attach\n", (int)getpid());
        raise(SIGSTOP);
    }
    if (do_init) do_init();

    time_t last_mtime = file_mtime(product);
    int host_pending = 0;   // defer mode: a source change is staged, awaiting the app's go
    pid_t compile_pid = 0;  // async recompile in flight — the app keeps running while it builds

    while (do_frame && do_frame()) {
        // isolated child died? publish the verdict for orbiter to render.
        // and relaunch it when orbiter asks (SILVER_ISOLATE_RESTART).
        isolate_reap();
        isolate_restart_check();

        // app<->host signals (resolved each iter — handle changes across reloads)
        au_live_set_pending_fn set_pending = (au_live_set_pending_fn)dlsym(handle, "au_live_set_pending");
        au_live_take_apply_fn  take_apply  = (au_live_take_apply_fn) dlsym(handle, "au_live_take_apply");
        au_live_get_defer_fn   get_defer   = (au_live_get_defer_fn)  dlsym(handle, "au_live_get_defer");
        au_live_get_defer_fn   get_reload  = (au_live_get_defer_fn)  dlsym(handle, "au_live_get_reload");
        // defer is dynamic: the app turns it on (au_live_set_defer); env forces it for devs.
        int defer = host_defer || (get_defer && get_defer());
        // reload can be turned OFF by the app (au_live_set_reload(0)) — orbiter does,
        // since it edits its own dependencies and would reload ITSELF otherwise.
        int reload_off = no_reload || (get_reload && !get_reload());

        // watch source files — when any .ag/.c changes
        int force = 0;
        int changed = 0;
        if (!reload_off)
        for (int i = 0; i < nsr; i++) {
            if (file_mtime(srcs[i].path) != srcs[i].mtime) {
                fprintf(stdout, "%s: source changed: %s\n", name, srcs[i].path);
                changed = 1;
            }
        }
        if (changed) {
            if (defer) {
                // refresh all source mtimes so we don't retrigger until the next edit
                for (int j = 0; j < nsr; j++)
                    srcs[j].mtime = file_mtime(srcs[j].path);
                // stage it: signal the app a reload is pending; recompile only on its request
                host_pending = 1;
                if (set_pending) set_pending(1);
            } else if (!compile_pid) {
                // recompile in the BACKGROUND — the app keeps running while silver
                // builds; the reap below reloads the instant the build lands (the
                // only pause left is the flash save + dlopen swap).
                for (int j = 0; j < nsr; j++)
                    srcs[j].mtime = file_mtime(srcs[j].path);
                compile_pid = rebuild_spawn(name, 0);
                if (compile_pid < 0) compile_pid = 0;
            }
            // else: a compile is already in flight — leave the mtimes stale so this
            // change retriggers a fresh build as soon as the current one is reaped
        }

        // reap an in-flight recompile without blocking the frame loop
        if (compile_pid) {
            int st = 0;
            pid_t r = waitpid(compile_pid, &st, WNOHANG);
            if (r == compile_pid) {
                compile_pid = 0;
                if (rebuild_status(st, name) == 0) {
                    force = 1;   // good build → save + reload below
                } else {
                    // a failed recompile must NOT swap in a broken module or take the app
                    // down — keep the live instance running on the last good build.
                    fprintf(stderr, "%s: recompile FAILED — keeping the running build "
                        "(fix the errors above; the app stays up)\n", name);
                }
            } else if (r < 0) {
                compile_pid = 0;
            }
        }

        // defer mode: the app asked us to apply the staged change → recompile + reload now
        if (defer && host_pending && take_apply && take_apply()) {
            host_pending = 0;
            if (set_pending) set_pending(0);
            int rc = do_recompile(handle, name);
            for (int j = 0; j < nsr; j++)
                srcs[j].mtime = file_mtime(srcs[j].path);
            if (rc == 0) {
                force = 1;
            } else {
                fprintf(stderr, "%s: recompile FAILED — keeping the running build\n", name);
            }
        }

        // watch product symlink — reload when .so is relinked (external builds).
        // while our own compile is in flight the product relinks BEFORE silver
        // finishes — the reap above owns that reload, so stand down until then.
        time_t cur = file_mtime(product);
        if (!reload_off && (force || (!compile_pid && cur != last_mtime))) {
            last_mtime = cur;
            fprintf(stderr, "%s: reloading\n", name);

            // Destroy the OLD instance FIRST. Its silver_live_destroy runs
            // module_erase_silver, clearing the old silver modules — so the
            // registry is CLEAN before the new .so's global constructors register
            // fresh types. (Previously the new .so was loaded first, its
            // constructors registered, and THEN this erase wiped the just-
            // registered new module — leaving find_type unable to resolve the
            // app's own element types on reload.)
            // SILVER_RELOAD_SAVE is set ONLY for the reload-path destroy (the final
            // exit destroy never sees it) — apps use it to flash-save live state.
            // SILVER_RELOAD_LOAD stays set afterward: every subsequent init in this
            // process IS a reload, so the fresh instance may restore the flash state.
            setenv("SILVER_RELOAD_SAVE", "1", 1);
            if (do_destroy) do_destroy();
            unsetenv("SILVER_RELOAD_SAVE");
            setenv("SILVER_RELOAD_LOAD", "1", 1);

            // Load the new .so — its constructors register into the cleared
            // registry. The old handle stays open until the dlclose below, so
            // shared dependency refcounts never hit zero during the swap.
            n = readlink(product, lib, sizeof(lib) - 1);
            if (n < 0) break;
            lib[n] = '\0';
            void* new_handle = reload_dlopen(lib, cur);
            // an external writer (an agent rebuilding sources) can relink the
            // .so while we copy it — the torn copy fails to load. wait for the
            // write to finish and recopy instead of dying.
            for (int rt = 0; !new_handle && rt < 10; rt++) {
                fprintf(stderr, "%s: reload copy torn — retrying (%d)\n", name, rt + 1);
                usleep(300000);
                n = readlink(product, lib, sizeof(lib) - 1);
                if (n < 0) break;
                lib[n] = '\0';
                new_handle = reload_dlopen(lib, file_mtime(product));
            }
            if (!new_handle) {
                fprintf(stderr, "%s: reload failed: %s\n", name, dlerror());
                return 1;
            }
            dlclose(handle);

            // Initialize new instance now that old is gone
            handle    = new_handle;
            do_init   = dlsym(handle, INIT_SYM);
            do_frame  = dlsym(handle, FRAME_SYM);
            do_destroy= dlsym(handle, DESTROY_SYM);
            stash_args(handle, argc, argv);
            if (do_init) do_init();
            fprintf(stderr, "%s: reload complete\n", name);

            // refresh source watch list from new artifacts
            load_sources(artifacts, srcs, &nsr);
        }
    }

    // orbiter closed: take the isolated child down with the window
    if (g_isolate_child) {
        kill(g_isolate_child, SIGTERM);
        waitpid(g_isolate_child, NULL, 0);
        g_isolate_child = 0;
    }
    if (do_destroy) do_destroy();
    if (handle)     dlclose(handle);
    return 0;
}
