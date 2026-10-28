#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <posix.h>   // the posix surface windows lacks, in one header
#else
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/resource.h>
#include <libgen.h>
#include <signal.h>
#include <execinfo.h>
#include <sys/wait.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#ifdef __linux__
#include <sys/prctl.h>
#include <ucontext.h>
#endif
#include <fcntl.h>
#endif
#include <sys/stat.h>
#ifndef _WIN32
// posix.h above carries dirent and flock where these do not exist
#include <dirent.h>
#include <sys/file.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <sys/ucontext.h>
#endif
#include <time.h>    // time() — the future-mtime clamp in sources_newer
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

extern void path_set_share_name(const char* name);

#ifndef _WIN32
extern char** environ;   // on windows posix.h aliases it to the crt's live block
// windows declares this in posix.h and links it from Au; this host links
// neither, so the one rule is restated here -- logs go with the build, never
// the system temp folder, which gets swept out from under a run
#ifdef SILVER_ROOT
#define HOST_TMP SILVER_ROOT "/install/tmp"
#else
#define HOST_TMP "/tmp"
#endif
static const char* temp_dir(void) {
    static int made = 0;
    if (!made) { made = 1; mkdir(HOST_TMP, 0755); }
    return HOST_TMP;
}
#endif

static void symbolize_crash_log(const char* appname);

// ---- app hosting channel -----------------------------------------------------
// ONE anonymous memfd (RAM-backed shared memory, no filesystem name, no
// socket, no file — EVER) created by this process and inherited by every
// child we spawn (SILVER_SHM_FD). it holds a table of app slots: two message
// rings + a row of shared-texture descriptors each. apps publish the dma-buf fd
// of the screen texture they already render into; consumers pull it with
// pidfd_getfd and import the same GPU memory. this process only spawns and
// reaps — it never touches pixels or fds. layout MIRRORS trinity.cc.
#define HM_SLOTS   1024
#define HOST_APPS  8
#define HOST_TEX   10
#define HOST_AUDIO 49152        // stereo frames of sound per slot (~1 s at 48 kHz)
typedef struct { int32_t type, a, b, c; } HostMsg;
typedef struct { volatile uint32_t head, tail; HostMsg m[HM_SLOTS]; } HostRing;
typedef struct { volatile int32_t pid, fd0, fd1, front, gen, width, height, format; } SharedTex;
typedef struct {
    HostRing  to_ide, to_app;
    SharedTex tex[HOST_TEX];    // 0 app screen, 1 ide overlay, 2.. instruments
    // the app's sound as it plays (AudioOut tees every write): stereo i16 at
    // audio_rate, so a recording on the ide side carries the pane's audio
    volatile int32_t  audio_rate;
    volatile uint32_t audio_w, audio_r;
    // orbiter plays the ring: its read index, and 1 while it plays
    volatile uint32_t audio_p;
    volatile int32_t  audio_play;
    int16_t  audio[HOST_AUDIO * 2];
    volatile int32_t app_pid;   // process bound to this slot
    volatile int32_t state;     // 0 free, 1 spawn requested, 2 live, 3 exited
    volatile int32_t verdict;   // 0 unset, >0 exit code+1, <0 -signal, -1000 build failed
    volatile int32_t flags;     // HOST_APP_* launch flags; never in the name
    char name[1024];            // "module [flags] [default-arg]" to spawn
} HostApp;
#define HOST_APP_DEBUG       1  // the app stops before init so orbiter can attach lldb
#define HOST_APP_CLEAN       2  // a full --clean rebuild before the spawn
#define HOST_APP_DEBUG_BUILD 4  // built -O0 -g, so the debugger sees source lines
#define HOST_APP_DISPLAY_SHIFT 3 // bits 3-4: trinity's Display (0 pip, 1 full, 2 window, 3 screen)
#define HOST_APP_HZ_SHIFT      8 // bits 8-15: the host display's refresh rate
#define HOST_APP_COVERAGE (1 << 16) // silver --coverage; the run writes coverage.lcov
typedef struct {
    volatile int32_t host_pid;  // this supervisor; spawn requests SIGUSR1 it
    HostApp app[HOST_APPS];
} HostShared;

static int rebuild_blocking(const char* name, int clean);
#if defined(__linux__)
// a bound app's line to its holder, which runs its builds
static int g_broker = -1;
static int broker_build_wait(int block, int* st);
#endif
// a module outside silver's folder: the path silver builds
static const char* g_build_path = NULL;

typedef int        (*frame_fn)(void);
typedef void       (*destroy_fn)(void);
typedef void       (*init_fn)(void);
typedef int        (*au_compile_ready_fn)(void);
typedef void       (*au_compile_invoke_fn)(const char*);
typedef void       (*au_main_args_fn)(int, char**);
typedef void       (*au_live_set_pending_fn)(int);
typedef int        (*au_live_take_apply_fn)(void);
typedef int        (*au_persist_fn)(void*);
typedef int        (*module_purge_image_fn)(void*);
typedef int        (*watch_pause_image_fn)(void*);
typedef int        (*async_wait_image_fn)(void*);
typedef int        (*au_live_get_defer_fn)(void);

// stash the process argv into libAu (loaded inside the app .so) so silver_live_init
// can parse the app's command-line flags into its instance. safe no-op on old libs.
static void (*g_leak_report)(void);

static void stash_args(void* handle, int argc, char** argv) {
    (void)handle;
    // au_main_args / au_leak_report live in Au.dll, not the app module. on
    // windows GetProcAddress(app, ...) never crosses into a dependency dll, so
    // the app arg-stash silently no-op'd and every app booted with no argv
    // (rom empty -> black). RTLD_DEFAULT scans every loaded module, so it finds
    // the one Au.dll the app actually reads its g_main_argv from.
    au_main_args_fn set_args = (au_main_args_fn)dlsym(RTLD_DEFAULT, "au_main_args");
    if (set_args) set_args(argc, argv);
    if (!g_leak_report)
        g_leak_report = (void(*)(void))dlsym(RTLD_DEFAULT, "au_leak_report");
}
#define FRAME_SYM   "silver_live_frame"
#define DESTROY_SYM "silver_live_destroy"
#define INIT_SYM    "silver_live_init"

typedef void       (*au_space_begin_fn)(void*);
typedef void*      (*au_space_detach_fn)(void);
typedef void       (*au_space_promote_fn)(void*);
typedef void       (*au_auto_free_fn)(void);

// a parallel reload: the new image's init runs here while the live
// instance keeps framing on the main thread
typedef struct {
    pthread_t thread;
    void*     handle;
    void*     space;
    int       argc;
    char**    argv;
    int       state;   // 0 idle, 1 loading, 2 ready for the switch
    long      ready_at;   // when the app was told (pending 3)
} reload_job_t;
static reload_job_t reload_job;
static void stash_args(void* handle, int argc, char** argv);
static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
}

typedef void  (*module_erase_fn)(void*, const char*);
typedef void* (*find_module_fn)(const char*);

// the old instance's teardown, off the main thread: destroy, wait its
// worker threads out of the image, close the image
typedef struct {
    pthread_t   thread;
    void*       handle;
    destroy_fn  destroy;
    void*       image;
    const char* name;
    int         state;   // 0 idle, 1 running
    int         done;
    int         destroyed;   // the instance is gone; its image still mapped
    int         drained;     // the main thread drained its pool after that
} close_job_t;
static close_job_t close_job;

static void* close_worker(void* arg) {
    close_job_t* job = (close_job_t*)arg;
    long t0 = now_ms();
    if (job->destroy) job->destroy();
    // this thread's pool: the teardown's temporaries, freed while mapped
    { au_auto_free_fn af = (au_auto_free_fn)dlsym(RTLD_DEFAULT, "auto_free"); if (af) af(); }
    long t1 = now_ms();
    async_wait_image_fn await = (async_wait_image_fn)dlsym(job->handle, "async_wait_image");
    if (await) await(job->image);
    long t2 = now_ms();
    // objects this teardown dropped to zero while they sat in the main
    // thread's pool are freed by that thread's next drain: their types
    // are in this image, so it stays mapped until that drain has run
    __atomic_store_n(&job->destroyed, 1, __ATOMIC_RELEASE);
    while (!__atomic_load_n(&job->drained, __ATOMIC_ACQUIRE)) usleep(1000);
    dlclose(job->handle);
    fprintf(stderr, "[%s] old instance closed: destroy %ldms workers %ldms close %ldms\n",
        job->name, t1 - t0, t2 - t1, now_ms() - t2);
    __atomic_store_n(&job->done, 1, __ATOMIC_RELEASE);
    return NULL;
}

// a running close ends before its job is reused: drain for it, then join
static void close_job_finish(void) {
    while (!__atomic_load_n(&close_job.destroyed, __ATOMIC_ACQUIRE)) usleep(1000);
    if (!close_job.drained) {
        au_auto_free_fn afree = (au_auto_free_fn)dlsym(RTLD_DEFAULT, "auto_free");
        if (afree) afree();
        __atomic_store_n(&close_job.drained, 1, __ATOMIC_RELEASE);
    }
    pthread_join(close_job.thread, NULL);
    close_job.state = close_job.done = close_job.destroyed = close_job.drained = 0;
}

static void* reload_worker(void* arg) {
    reload_job_t* job = (reload_job_t*)arg;
    au_space_begin_fn  sbegin  = (au_space_begin_fn)dlsym(RTLD_DEFAULT, "au_space_begin");
    au_auto_free_fn    scapt   = (au_auto_free_fn)dlsym(RTLD_DEFAULT, "au_space_capture");
    au_space_detach_fn sdetach = (au_space_detach_fn)dlsym(RTLD_DEFAULT, "au_space_detach");
    au_auto_free_fn    afree   = (au_auto_free_fn)dlsym(RTLD_DEFAULT, "auto_free");
    if (sbegin) sbegin(job->handle);
    if (scapt)  scapt();
    stash_args(job->handle, job->argc, job->argv);
    init_fn init = (init_fn)dlsym(job->handle, INIT_SYM);
    if (init) init();
    if (afree) afree();   // this thread's pool: refs-0 leftovers of the init
    job->space = sdetach ? sdetach() : NULL;
    __atomic_store_n(&job->state, 2, __ATOMIC_RELEASE);
    return NULL;
}

#define MAX_SOURCES 128

// ---- the recording's microphone -----------------------------------------
// the capture thread is the host's: a reload swaps the app, never the mic.
// the device's mono samples go round a ring; each instance reads it with
// its own cursor (au_live_mic_read), so the take hears no gap.
typedef bool (*mic_open_fn)(int*);
typedef int  (*mic_read_fn)(int16_t*, int);
typedef void (*mic_close_fn)(void);
#define MIC_RING (1 << 18)   // frames: 5.4 s at 48 kHz
static int16_t   g_mic_ring[MIC_RING];
static int64_t   g_mic_written = 0;   // frames ever written
static int       g_mic_rate    = 0;   // 0 opening, -1 none, else Hz
static int       g_mic_started = 0;
static pthread_t g_mic_thread;

static void* mic_loop(void* arg) {
    (void)arg;
    mic_open_fn  mopen  = (mic_open_fn) dlsym(RTLD_DEFAULT, "platform_mic_open");
    mic_read_fn  mread  = (mic_read_fn) dlsym(RTLD_DEFAULT, "platform_mic_read");
    mic_close_fn mclose = (mic_close_fn)dlsym(RTLD_DEFAULT, "platform_mic_close");
    int rate = 0;
    if (!mopen || !mread || !mopen(&rate) || rate <= 0) {
        fprintf(stderr, "silver-host: record: no microphone\n");
        __atomic_store_n(&g_mic_rate, -1, __ATOMIC_RELEASE);
        return NULL;
    }
    fprintf(stderr, "silver-host: record: microphone on at %d Hz\n", rate);
    __atomic_store_n(&g_mic_rate, rate, __ATOMIC_RELEASE);
    int16_t buf[480];
    for (;;) {
        int got = mread(buf, 480);
        if (got < 0) break;
        if (got == 0) { usleep(2000); continue; }
        int64_t w = __atomic_load_n(&g_mic_written, __ATOMIC_RELAXED);
        for (int i = 0; i < got; i++) g_mic_ring[(w + i) & (MIC_RING - 1)] = buf[i];
        __atomic_store_n(&g_mic_written, w + got, __ATOMIC_RELEASE);
    }
    if (mclose) mclose();
    __atomic_store_n(&g_mic_rate, -1, __ATOMIC_RELEASE);
    return NULL;
}

// the rate once open: 0 while it opens, -1 with no mic
static int host_mic_start(void) {
    if (!g_mic_started) {
        g_mic_started = 1;
        if (pthread_create(&g_mic_thread, NULL, mic_loop, NULL) != 0) {
            g_mic_rate = -1;
            return -1;
        }
        pthread_detach(g_mic_thread);
    }
    return __atomic_load_n(&g_mic_rate, __ATOMIC_ACQUIRE);
}

// frames since *cursor (-1: from now); a reader that fell a ring behind
// skips ahead to the oldest frame still held
static int host_mic_read(int16_t* out, int max, int64_t* cursor) {
    int64_t w = __atomic_load_n(&g_mic_written, __ATOMIC_ACQUIRE);
    if (*cursor < 0 || *cursor > w) *cursor = w;
    if (w - *cursor > MIC_RING) *cursor = w - MIC_RING;
    int n = (int)((w - *cursor < max) ? w - *cursor : max);
    for (int i = 0; i < n; i++) out[i] = g_mic_ring[(*cursor + i) & (MIC_RING - 1)];
    *cursor += n;
    return n;
}
typedef void (*au_live_mic_hooks_fn)(int (*)(void), int (*)(int16_t*, int, int64_t*));

static const char* g_app_name = "app";
static int         g_log_slot = -1;   // hosted slot > 0 logs to <app>.<slot>.log
static void app_log_path(char* out, size_t cap, const char* name, int slot) {
    if (slot > 0) snprintf(out, cap, "%s/%s.%d.log", temp_dir(), name, slot);
    else          snprintf(out, cap, "%s/%s.log",    temp_dir(), name);
}

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
#define IDE_ENV            "IN_IDE"
// the configuration this host was built for. a debug build names its products
// apart from a release one, and this is how the host finds its own chain
#ifndef SILVER_BUILD_TAG
#define SILVER_BUILD_TAG ""
#endif

static char  g_isolate_cwd[4096];
static char  g_exe_path[4096];       // this binary, resolved in main
static int   g_argc;
static char** g_argv;

// the app<->ide channel: an anonymous memfd (RAM-backed shared memory, no
// filesystem name, no socket) created once here and inherited by every
// process we spawn. its number in each child is published as SILVER_SHM_FD.
static int         g_shm_fd = -1;
// each slot's spawned process: the one its exit is read from
static pid_t       g_slot_proc[HOST_APPS];
static HostShared* g_shm    = NULL;
#define SILVER_SHM_CHILD_FD 21   // fixed number the channel lands on in children

#ifndef __linux__
// no memfd here: an unlinked temp file is the same anonymous, inheritable fd
static int memfd_create(const char* name, unsigned flags) {
    char p[] = "/tmp/silver-shm-XXXXXX";
    int fd = mkstemp(p);
    if (fd >= 0) unlink(p);
    return fd;
}
#endif

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
#ifdef _WIN32
    // handle inheritance carries a HANDLE, not a numbered descriptor, so the
    // dup2 above cannot land the channel on a fixed fd. advertising one the
    // child does not have made it mmap a bogus descriptor
    (void)fa;
    snprintf(fdenv, cap, "SILVER_SHM_FD=-1");
#else
    snprintf(fdenv, cap, "SILVER_SHM_FD=%d", g_shm_fd >= 0 ? SILVER_SHM_CHILD_FD : -1);
#endif
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
#ifndef _WIN32
    return 1;
#else
    return 0;
#endif
}

// nothing forwards output from here: this process is linked /SUBSYSTEM:WINDOWS
// and its stdout/stderr report -2 (no association), so every write fails EBADF.
// silver owns the console and tails the app log there instead

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
    // /proc is linux only: the same binary is what main resolved
    int sp = posix_spawn(&pid, g_exe_path[0] ? g_exe_path : "/proc/self/exe", &fa, NULL, cargv, cenv);
    posix_spawn_file_actions_destroy(&fa);
    free(cargv);
    free(cenv);
    if (sp != 0) {
        fprintf(stderr, "silver-host: isolate spawn failed: %s (exe %s, cwd %s)\n", strerror(sp),
            g_exe_path[0] ? g_exe_path : "/proc/self/exe", g_isolate_cwd[0] ? g_isolate_cwd : "-");
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
    char name[1024];
    strncpy(name, (const char*)ap->name, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    if (!name[0]) { ap->state = 3; return; }
    // "module [args...]": everything after the module rides the spawn argv
    char* arg = strchr(name, ' ');
    if (arg) { *arg = 0; arg++; }
    // split the tail on spaces — an app may take several args, not just a doc
    #define MAX_APP_ARGS 64
    char* app_args[MAX_APP_ARGS];
    int   n_app_args = 0;
    for (char* p = arg; p && *p && n_app_args < MAX_APP_ARGS; ) {
        while (*p == ' ') p++;
        if (!*p) break;
        app_args[n_app_args++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    int   dbg   = (ap->flags & HOST_APP_DEBUG)       != 0;
    int   clean = (ap->flags & HOST_APP_CLEAN)       != 0;
    int   dbgb  = (ap->flags & HOST_APP_DEBUG_BUILD) != 0;
    int   cov   = (ap->flags & HOST_APP_COVERAGE)    != 0;
    int   disp  = (ap->flags >> HOST_APP_DISPLAY_SHIFT) & 3;
    static const char* dnames[] = { "PIP", "Embed", "Window", "Screen" };
    char* nm    = name;
    // a folder path: silver builds it, its last word names the app
    char* sl    = strrchr(name, '/');
    if (sl) { g_build_path = name; nm = sl + 1; }
    // the build type rides in the environment: this build, and the app's own
    // rebuilds when its sources change, all read it
    if (dbgb) setenv("SILVER_DEBUG_BUILD", "1", 1);
    else      unsetenv("SILVER_DEBUG_BUILD");
    // one coverage map: the last run's, cleared as this one starts
    static char covenv[4200];
    snprintf(covenv, sizeof(covenv), "SILVER_COVERAGE_LCOV=%s/install/tmp/coverage.lcov", SILVER_ROOT);
    if (cov) {
        setenv("SILVER_COVERAGE_BUILD", "1", 1);
        unlink(covenv + strlen("SILVER_COVERAGE_LCOV="));
    } else
        unsetenv("SILVER_COVERAGE_BUILD");
    // app binaries live beside this supervisor binary (one products dir)
    char bin[4300];
    snprintf(bin, sizeof(bin), "%s/%s%s", bindir, nm, dbgb ? "-dbg" : "");
    // fresh app log for this run; the app APPENDS (host_log_setup honors the slot),
    // so the build output below + the app's runtime both land here for the console.
    { char lp[256]; app_log_path(lp, sizeof(lp), nm, k);
      int lfd = open(lp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (lfd >= 0) close(lfd); }
    // always build before a run: the host has no staleness view of a slot
    // app's sources, and silver's own cache no-ops when nothing changed
    g_log_slot = k;
    int built = rebuild_blocking(nm, clean);
    g_log_slot = -1;
    g_build_path = NULL;
    if (built != 0) {
        fprintf(stderr, "silver-host: %s build failed — slot %d dead\n", name, k);
        ap->verdict = -1000;
        ap->state = 3;
        return;
    }
    char* cargv[MAX_APP_ARGS + 4];
    int   ca = 0;
    cargv[ca++] = bin;
    // every instance owns a window: pip and full keep it hidden and publish
    // frames to the slot, so a display change never restarts the process
    for (int i = 0; i < n_app_args; i++) cargv[ca++] = app_args[i];
    cargv[ca] = NULL;
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    if (g_isolate_cwd[0]) posix_spawn_file_actions_addchdir_np(&fa, g_isolate_cwd);
    int in_fd = -1;
#ifndef _WIN32
    // its standard input: a pipe beside its log (<name>.<slot>.in) the
    // ide writes typed lines into; read-write, so it never blocks or ends
    { char ip[256]; snprintf(ip, sizeof(ip), "%s/%s.%d.in", temp_dir(), nm, k);
      unlink(ip);
      if (mkfifo(ip, 0600) == 0) in_fd = open(ip, O_RDWR);
      if (in_fd >= 0) posix_spawn_file_actions_adddup2(&fa, in_fd, 0); }
#endif
    static char fdenv[32];
    shm_inherit(&fa, fdenv, sizeof(fdenv));
    static char slotenv[32];
    snprintf(slotenv, sizeof(slotenv), "SILVER_APP_SLOT=%d", k);
    int n = 0;
    while (environ[n]) n++;
    char** cenv = malloc((n + 9) * sizeof(char*));
    // orbiter's own isolate marker stays here: an app that inherits it
    // skips its own supervision and never reports to us
    int ce = 0;
    for (int i = 0; i < n; i++)
        if (strncmp(environ[i], "SILVER_ISOLATE", 14) != 0) cenv[ce++] = environ[i];
    // an instance is ours: no supervisor of its own
    cenv[ce++] = (char*)"SILVER_ISOLATE=0";
    cenv[ce++] = fdenv;
    cenv[ce++] = slotenv;
    // the display rides in the env: the app's args parse on its element,
    // which does not carry trinity's props
    static char dispenv[32];
    snprintf(dispenv, sizeof(dispenv), "SILVER_DISPLAY=%s", dnames[disp]);
    cenv[ce++] = dispenv;
    static char hzenv[32];
    snprintf(hzenv, sizeof(hzenv), "SILVER_HZ=%d", (ap->flags >> HOST_APP_HZ_SHIFT) & 255);
    cenv[ce++] = hzenv;
    if (dbg)  cenv[ce++] = (char*)"SILVER_DEBUG=1";
    if (dbgb) cenv[ce++] = (char*)"SILVER_DEBUG_BUILD=1";
    if (cov)  cenv[ce++] = covenv;
    cenv[ce] = NULL;
    pid_t pid = 0;
    int rc = posix_spawn(&pid, bin, &fa, NULL, cargv, cenv);
    posix_spawn_file_actions_destroy(&fa);
    if (in_fd >= 0) close(in_fd);
    free(cenv);
    if (rc != 0) {
        fprintf(stderr, "silver-host: spawn %s failed: %s\n", name, strerror(rc));
        ap->verdict = -1000;
        ap->state = 3;
        return;
    }
    // a bound app puts its own pid here
    g_slot_proc[k] = pid;
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

// a slot request wakes the supervisor with SIGUSR1: the signal only cuts
// the poll wait short — the request itself is state 1 in the slot
static void on_sigusr1(int sig) { (void)sig; }
// the supervised peer (slot 0) — the process that owns the window
static pid_t g_peer_pid  = 0;

// everything this host spawned dies with it — every exit path, signals
// included. the peer prints its --leaks report on ITS way out (ctrl+c gave
// it a SIGINT of its own), and this host's tee drain must stay alive until
// that output lands — so wait first, nudge with TERM only if the peer got
// no signal, and KILL only a dead-hung peer. SIGCONT first: a crash-frozen
// peer sits in SIGSTOP where no signal can land.
static void host_wait_peer(pid_t p) {
    if (p <= 0 || kill(p, 0) != 0) return;
    kill(p, SIGCONT);
    for (int i = 0; i < 300; i++) {
        int st;
        if (waitpid(p, &st, WNOHANG) == p) return;
        if (kill(p, 0) != 0) return;
        if (i == 100) kill(p, SIGTERM);
        usleep(10000);
    }
    kill(p, SIGKILL);
}

static void host_shutdown(void) {
    slots_shutdown();
    host_wait_peer(g_peer_pid);
}

static void host_exit_signal(int sig) {
    host_shutdown();
    signal(sig, SIG_DFL);
    raise(sig);
}

// no console handler here: a /SUBSYSTEM:WINDOWS binary does not reliably get
// console control events. silver reads ctrl+c and ends the job we live in


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
    // the anonymous channel exists BEFORE any child, so all inherit the same
    // memfd. no file, no socket, no session-keyed path.
    shm_create();
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigusr1;          // no SA_RESTART: waitpid must EINTR
    sigaction(SIGUSR1, &sa, NULL);
    pid_t pid = isolate_spawn();
    if (pid < 0) return -1;
    g_peer_pid = pid;
    atexit(host_shutdown);
    signal(SIGINT,  host_exit_signal);
    signal(SIGTERM, host_exit_signal);
    signal(SIGHUP,  host_exit_signal);
    // slot 0 IS the peer process: a summoned orbiter debugs it through the
    // same slot table (AppView) every hosted pane app uses
    if (g_shm) {
        g_shm->app[0].app_pid = (int32_t)pid;
        g_shm->app[0].verdict = 0;
        g_shm->app[0].state   = 2;
    }
    for (;;) {
        // service spawn requests by POLLING the slots, not only on EINTR: a
        // SIGUSR1 landing anywhere but inside waitpid was lost until the next
        // child event — the request sat for minutes. the signal now only
        // shortens the poll tick; state==1 in a slot is the request itself.
        int spawned = 0;
        if (g_shm)
            for (int k = 1; k < HOST_APPS; k++)
                if (g_shm->app[k].state == 1) { spawn_slot_app(k, bindir); spawned = 1; }
        int   st = 0;
        // WUNTRACED: a frozen peer (crash handler raised SIGSTOP) reports here
        pid_t r  = waitpid(-1, &st, WNOHANG | WUNTRACED);
        if (r == 0 || (r < 0 && errno == EINTR)) {
            if (r == 0) usleep(30000);
            continue;
        }
        if (r < 0) {
            perror("silver-host: waitpid");
            *exit_code = 1;
            return 0;
        }
        if (WIFSTOPPED(st)) {
            // slot-app stops (the debug gate, F8 pauses) are not ours
            if (r != pid) continue;
            // the peer froze at a crash site (handler marked state 4): the
            // slot carries the verdict, the crash log is symbolized, and
            // the session ends here
            if (!g_shm || g_shm->app[0].state != 4) continue;
            int fsig = -g_shm->app[0].verdict;
            fprintf(stderr, "silver-host: peer frozen — signal %d (%s)\n",
                fsig, strsignal(fsig));
            symbolize_crash_log(appname);
            kill(pid, SIGCONT);
            waitpid(pid, &st, 0);
            g_shm->app[0].state = 3;
            slots_shutdown();
            g_peer_pid = 0;
            *exit_code = 128 + fsig;
            return 0;
        }
        if (r != pid) {                    // a hosted app exited
            if (g_shm)
                for (int k = 1; k < HOST_APPS; k++)
                    if (g_slot_proc[k] == r) {
                        g_slot_proc[k] = 0;
                        g_shm->app[k].verdict = WIFSIGNALED(st)
                            ? -WTERMSIG(st) : WEXITSTATUS(st) + 1;
                        g_shm->app[k].state = 3;
                    }
            continue;
        }
        // the peer ended (stop from the IDE, normal exit, or a hard kill).
        // record the verdict in the slot either way.
        if (g_shm) {
            g_shm->app[0].verdict = WIFSIGNALED(st)
                ? -WTERMSIG(st) : (WEXITSTATUS(st) + 1);
            g_shm->app[0].state   = 3;
        }
        if (WIFSIGNALED(st)) {
            fprintf(stderr, "silver-host: app ended: signal %d (%s)\n",
                WTERMSIG(st), strsignal(WTERMSIG(st)));
            symbolize_crash_log(appname);
        } else
            fprintf(stderr, "silver-host: app ended: exit %d\n", WEXITSTATUS(st));
        // the peer process is gone — the shared window went with it, so the
        // session ends. (stop in the IDE never lands here: it stops the
        // peer's WORLD in place, the process and window live on.)
        slots_shutdown();
        g_peer_pid = 0;
        *exit_code = WIFSIGNALED(st) ? 128 + WTERMSIG(st)
                   : (WIFEXITED(st) ? WEXITSTATUS(st) : 1);
        return 0;
    }
}

#ifdef __linux__
#include <sys/socket.h>
#include <sys/un.h>
// a crashed app is frozen with its streams open: the sound server
// waits on it and every program's sound stops. its connections to
// the server go first (getpeername and shutdown are signal-safe)
static void drop_sound_sockets(void) {
    for (int fd = 3; fd < 4096; fd++) {
        struct sockaddr_un sa;
        socklen_t n = sizeof(sa);
        if (getpeername(fd, (struct sockaddr*)&sa, &n) != 0) continue;
        if (sa.sun_family != AF_UNIX) continue;
        if (strstr(sa.sun_path, "pipewire") || strstr(sa.sun_path, "pulse"))
            shutdown(fd, SHUT_RDWR);
    }
}
#endif

static void crash_handler(int sig, siginfo_t* si, void* ucv) {
    void* frames[64];
    int   nframes = 0;
#ifdef __linux__
    drop_sound_sockets();
#endif
    // the faulting pc from the context is the one address that is always
    // right, and the unwinder below can itself fault (the guest did): say
    // the pc FIRST, straight to stderr, then unwind
#if defined(__linux__) && defined(__x86_64__)
    if (ucv) {
        void* pc = (void*)((ucontext_t*)ucv)->uc_mcontext.gregs[REG_RIP];
        Dl_info di;
        char    pl[256];
        int     pn;
        if (dladdr(pc, &di) && di.dli_fname && di.dli_fbase)
            pn = snprintf(pl, sizeof(pl), "%s: signal %d at %s +0x%lx\n", g_app_name, sig,
                di.dli_fname, (unsigned long)((char*)pc - (char*)di.dli_fbase));
        else
            pn = snprintf(pl, sizeof(pl), "%s: signal %d at 0x%lx (no module)\n", g_app_name, sig,
                (unsigned long)pc);
        if (pn > 0) (void)write(STDERR_FILENO, pl, (size_t)pn);
        frames[nframes++] = pc;
        // a call through a null pointer: the caller's return address is
        // the top of the stack, and the unwinder cannot see past pc 0
        if (!pc) {
            void** sp = (void**)((ucontext_t*)ucv)->uc_mcontext.gregs[REG_RSP];
            void*  ra = sp ? *sp : NULL;
            if (ra && dladdr(ra, &di) && di.dli_fname && di.dli_fbase) {
                pn = snprintf(pl, sizeof(pl), "%s: called from %s +0x%lx\n", g_app_name,
                    di.dli_fname, (unsigned long)((char*)ra - (char*)di.dli_fbase) - 1);
                if (pn > 0) (void)write(STDERR_FILENO, pl, (size_t)pn);
                frames[nframes++] = ra;
            }
        }
    }
#elif defined(__APPLE__) && defined(__aarch64__)
    // backtrace() sees nothing from a signal context here: the pc comes
    // from the thread state, then the callers by the frame-pointer chain
    // ([fp] = the caller's fp, [fp+8] = the return address into it)
    if (ucv) {
        ucontext_t* uc = (ucontext_t*)ucv;
        void*  pc = (void*)__darwin_arm_thread_state64_get_pc(uc->uc_mcontext->__ss);
        void*  lr = (void*)__darwin_arm_thread_state64_get_lr(uc->uc_mcontext->__ss);
        void** fp = (void**)__darwin_arm_thread_state64_get_fp(uc->uc_mcontext->__ss);
        Dl_info di;
        char    pl[256];
        int     pn;
        if (dladdr(pc, &di) && di.dli_fname && di.dli_fbase)
            pn = snprintf(pl, sizeof(pl), "%s: signal %d at %s +0x%lx\n", g_app_name, sig,
                di.dli_fname, (unsigned long)((char*)pc - (char*)di.dli_fbase));
        else
            pn = snprintf(pl, sizeof(pl), "%s: signal %d at 0x%lx (no module)\n", g_app_name, sig,
                (unsigned long)pc);
        if (pn > 0) (void)write(STDERR_FILENO, pl, (size_t)pn);
        frames[nframes++] = pc;
        // a leaf (or a call through a bad pointer) has not saved lr yet:
        // it is the caller, and the chain from fp continues above it
        if (lr && (!fp || fp[1] != lr)) frames[nframes++] = lr;
        for (int i = 0; fp && i < 60 && nframes < 62; i++) {
            if (((uintptr_t)fp & 7) || (uintptr_t)fp < 0x1000) break;
            void*  ra   = fp[1];
            void** next = (void**)fp[0];
            if (!ra) break;
            frames[nframes++] = ra;
            if (next <= fp) break;
            fp = next;
        }
    }
#endif
    (void)si;
    nframes += backtrace(frames + nframes, 64 - nframes);
    // Au keeps the last strings it freed; ask it to name them, if it is
    // in this process (the host does not link it)
    void (*notes)(void) = (void (*)(void))dlsym(RTLD_DEFAULT, "au_crash_notes");
    if (notes) notes();
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
    app_log_path(lp, sizeof(lp), g_app_name, g_log_slot);
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
            } else { // no module owns it: a jump through a bad pointer
                char fl[64];
                int  fn = snprintf(fl, sizeof(fl), "  @ ? 0x%lx\n", (unsigned long)frames[i]);
                if (fn > 0) (void)write(lf, fl, (size_t)fn);
            }
        }
        fsync(lf);
        close(lf);
    }
    (void)write(STDERR_FILENO, hdr, (size_t)hl);
    // --leaks summary still prints when the app dies by signal
    if (g_leak_report) g_leak_report();
    // peer freeze: a supervised app STOPS at the crash site instead of dying.
    // the supervisor marks the slot frozen, orbiter shows the last frame as
    // paused and lldb can attach to the stopped pid. a later SIGCONT (resume/
    // relaunch) falls through and the original signal kills it properly.
    if (getenv(ISOLATE_CHILD_ENV)) {
        if (g_shm) {
            g_shm->app[0].verdict = -sig;
            g_shm->app[0].state   = 4;
        }
#ifdef __linux__
        prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
#endif
        raise(SIGSTOP);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

// resolve the "  @ module +0xoff" frames the crash handler left in
// /tmp/<app>.log to file:line (addr2line reads the module's DWARF);
// runs in the supervisor AFTER the child died — popen is safe here
static void symbolize_crash_log(const char* appname) {
    if (!appname || !*appname) return;
    char lp[256];
    snprintf(lp, sizeof(lp), "%s/%s.log", temp_dir(), appname);
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

// the silver libraries this process loaded at launch, and their times:
// a reload swaps the app's library alone, so one of them rebuilt since
// (trinity) leaves the new app reading the old one's layout
#define HOST_LIBS 64
static char   host_lib_path[HOST_LIBS][512];
static time_t host_lib_time[HOST_LIBS];
static int    host_lib_n;

static void host_libs_note(const char* app_lib) {
#ifdef __linux__
    FILE* m = fopen("/proc/self/maps", "r");
    if (!m) return;
    char line[1024];
    while (fgets(line, sizeof(line), m) && host_lib_n < HOST_LIBS) {
        char* p = strchr(line, '/');
        if (!p) continue;
        p[strcspn(p, "\n")] = 0;
        if (!strstr(p, "/libsilver-") || strstr(p, "hotreload_")) continue;
        if (app_lib && strcmp(p, app_lib) == 0) continue;
        int seen = 0;
        for (int i = 0; i < host_lib_n; i++)
            if (strcmp(host_lib_path[i], p) == 0) seen = 1;
        if (seen) continue;
        snprintf(host_lib_path[host_lib_n], 512, "%s", p);
        host_lib_time[host_lib_n++] = file_mtime(p);
    }
    fclose(m);
#endif
}

// the first library rebuilt since launch, else null
static const char* host_libs_changed(void) {
    for (int i = 0; i < host_lib_n; i++) {
        time_t t = file_mtime(host_lib_path[i]);
        if (t && t != host_lib_time[i]) return host_lib_path[i];
    }
    return NULL;
}

// open fds, memory mappings, resident memory: what a failing alloc is short of
static void host_resources(const char* name, const char* when) {
    int fds = 0, maps = 0;
    DIR* d = opendir("/proc/self/fd");
    if (d) { while (readdir(d)) fds++; closedir(d); }
    FILE* m = fopen("/proc/self/maps", "r");
    if (m) { int c; while ((c = fgetc(m)) != EOF) if (c == '\n') maps++; fclose(m); }
    long rss_kb = 0;
    FILE* s = fopen("/proc/self/status", "r");
    if (s) {
        char line[256];
        while (fgets(line, sizeof(line), s))
            if (sscanf(line, "VmRSS: %ld", &rss_kb) == 1) break;
        fclose(s);
    }
    fprintf(stderr, "%s: %s fds=%d maps=%d rss=%ldMB\n", name, when, fds, maps, rss_kb / 1024);
}

static void host_cd(const char* dir) {
    fprintf(stderr, "cd: host -> %s\n", dir);
    chdir(dir);
}

static void cd_share(const char* bindir, const char* app) {
    char share[4096];
    const char* name = app;
#ifdef SILVER_SHARE_NAME
    name = SILVER_SHARE_NAME;
#endif
    snprintf(share, sizeof(share), "%s/../share/%s", bindir, name);
    struct stat st;
    if (stat(share, &st) == 0 && S_ISDIR(st.st_mode)) {
        host_cd(share);
        return;
    }
    // a system install names the share after the app, as the bin is
    snprintf(share, sizeof(share), "%s/../share/%s", bindir, app);
    if (stat(share, &st) == 0 && S_ISDIR(st.st_mode))
        host_cd(share);
}

static void* try_dlopen(const char* lib) {
#if defined(__SANITIZE_ADDRESS__) || (defined(__has_feature) && __has_feature(address_sanitizer)) || !defined(RTLD_DEEPBIND)
    // RTLD_DEEPBIND is a glibc extension — not available on macOS
    int flags = RTLD_NOW | RTLD_GLOBAL;
#else
    int flags = RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND;
#endif
    return dlopen(lib, flags);
}

// dlopen caches by dev+inode. If the linker overwrites the .so in-place (same
// inode), dlopen returns the old cached handle. Copy to a unique /tmp path to
// guarantee a fresh inode on every hot-reload.
static void* reload_dlopen(const char* lib, time_t ts) {
    char tmp[4096];
#ifdef _WIN32
    // /tmp is not a path here, and the loader does not care about the suffix.
    // the copy matters more on windows than unix: the build CANNOT relink a
    // dll this process has mapped, so we must never hold the product itself
    snprintf(tmp, sizeof(tmp), "%s/hotreload_%ld.dll", temp_dir(), (long)ts);
#else
    snprintf(tmp, sizeof(tmp), "/tmp/hotreload_%ld.so", (long)ts);
#endif
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
#ifndef _WIN32
        unlink(tmp);  // unlink immediately — dlopen holds the inode open
#endif                // windows refuses to delete a mapped dll; it is left behind
        return h;
    }
    if (src) fclose(src);
    if (dst) { fclose(dst); unlink(tmp); }
    // the product is mid-relink: opening its path hands back the
    // image already mapped, so the caller retries the copy instead
    if (!src) return NULL;
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
    // a source dated far in the future (a wrong system clock stamped it) would
    // be newer than every product forever -> endless rebuild. over 20 min ahead
    // is a bad clock, not an edit; ignore it. small skew still counts.
    time_t horizon = time(NULL) + 20 * 60;
    for (int i = 0; i < nsr; i++) {
        time_t sm = file_mtime(srcs[i].path);
        if (sm > horizon) continue;
        if (sm > prod) return 1;
    }
    return 0;
}

// recompile synchronously (no '&') so the fresh product is in place before load.
// run from SILVER_ROOT so silver resolves the module via <name>/ rather
// than relative to wherever the host was launched / cd_share'd to.
// returns 0 on a clean build, non-zero if silver failed or crashed. callers must
// NOT run the (stale) product when this fails.
// spawn the recompile WITHOUT waiting — the frame loop keeps the app live while
// silver builds; the caller reaps with waitpid(WNOHANG) and reloads on success.
static pid_t rebuild_spawn(const char* name, int clean) {
    char cmd[8192];
    const char* what = g_build_path ? g_build_path : name;
    // a debug build (-O0 -g) when the launch asked for one: per build, nothing sticky
    const char* dbgb = getenv("SILVER_DEBUG_BUILD") ? " --debug" : "";
    const char* covb = getenv("SILVER_COVERAGE_BUILD") ? " --coverage" : "";
#if defined(__linux__)
    // a bound app cannot write silver's folder: its holder builds
    if (g_broker >= 0) {
        char rq[4200];
        int rn = snprintf(rq, sizeof(rq), "build %d %s\n", clean, what);
        return write(g_broker, rq, (size_t)rn) == rn ? 1 : -1;
    }
#endif
    // --build: compile ONLY. bare `silver <app>` would LAUNCH the app (silver_live_run execs
    // the live host), spawning a whole second process+window on every reload while this one
    // keeps running. we just want the fresh .so produced so the host below hot-swaps it.
#ifdef _WIN32
    // cmd.exe will not run a program path spelled with forward slashes, and
    // SILVER_ROOT is spelled that way, so hand it a backslash copy
    char root[1024];
    snprintf(root, sizeof(root), "%s", SILVER_ROOT);
    for (char* p = root; *p; p++) if (*p == '/') *p = '\\';
    // gen.py puts the app in install\build; install\bin\silver is the symlink
    // `make install` makes, and that step is unix-only
    // silver's own flags come BEFORE the module name; anything after it is
    // handed to the app, so a trailing --build would launch instead of compile
    // SILVER_DEBUG would stop the build's export run for good
    snprintf(cmd, sizeof(cmd),
        "set \"SILVER_DEBUG=\" && cd /d \"%s\" && \"%s\\install\\build\\silver.exe\" --build%s%s%s %s",
        root, root, clean ? " --clean" : "", dbgb, covb, what);
#else
    // silver's flags before the module: after it they are the app's,
    // and silver would launch it with them instead of building
    snprintf(cmd, sizeof(cmd),
        "unset SILVER_DEBUG; cd \"" SILVER_ROOT "\" && \"" SILVER_ROOT "/install/bin/silver\" --build%s%s%s %s",
        clean ? " --clean" : "", dbgb, covb, what);
#endif
    // send the compile output to the app's OWN log so orbiter's console (which tails
    // /tmp/<app>.log) shows the compilation. spawn_slot_app truncated it beforehand,
    // and the app appends (host_log_setup) so build + runtime share the one file.
    char lp[256];
    app_log_path(lp, sizeof(lp), name, g_log_slot);
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
#ifdef _WIN32
    // no /bin/sh here; COMSPEC names the shell and it spells the flag /c
    char* shell = getenv("COMSPEC");
    if (!shell || !*shell) shell = "C:/Windows/System32/cmd.exe";
    char* sh_argv[] = { shell, "/c", cmd, NULL };
#else
    char* shell = "/bin/sh";
    char* sh_argv[] = { shell, "-c", cmd, NULL };
#endif
    pid_t pid = 0;
    int sp = posix_spawn(&pid, shell, &fa, NULL, sh_argv, environ);
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
#if defined(__linux__)
    if (g_broker >= 0) {
        broker_build_wait(1, &rc);
        return rebuild_status(rc, name);
    }
#endif
    pid_t w;
    do { w = waitpid(pid, &rc, 0); } while (w < 0 && errno == EINTR);
    if (w < 0) {
        fprintf(stderr, "%s: BUILD ERROR — waitpid failed\n", name);
        return -1;
    }
    return rebuild_status(rc, name);
}

#if defined(__linux__)
#include <sys/mount.h>
// orbiter-os: this host IS init. the kernel hands us a bare rootfs,
// so mount what a process expects before anything else looks
// init must never return: the kernel panics and its trace scrolls the
// real message off the screen. hold the console instead
static void init_console_bind(int on);

static void init_hold(void) {
    if (getpid() != 1) return;  // a forked helper exiting is not init
    fflush(stdout);              // exit flushes AFTER atexit: do it now
    fflush(stderr);
    fprintf(stderr, "init: orbiter exited; holding the console\n");
    // the screen: the app's drm lease closed with it, so the text console
    // has the display again. show the tail of the app log there
    init_console_bind(1);
    int tty = open("/dev/tty0", O_WRONLY);
    char lp[256];
    app_log_path(lp, sizeof(lp), g_app_name, g_log_slot);
    int lf = open(lp, O_RDONLY);
    fprintf(stderr, "init: app log %s %s\n", lp, lf >= 0 ? "" : "MISSING");
    if (lf >= 0) {
        off_t end = lseek(lf, 0, SEEK_END);
        lseek(lf, end > 6000 ? end - 6000 : 0, SEEK_SET);
        char buf[4096];
        ssize_t n;
        while ((n = read(lf, buf, sizeof(buf))) > 0) {
            if (tty >= 0) (void)write(tty, buf, (size_t)n);
            (void)write(STDERR_FILENO, buf, (size_t)n);   // serial too
        }
        close(lf);
    }
    if (tty >= 0) {
        const char* m = "\ninit: orbiter exited; holding the console\n";
        (void)write(tty, m, strlen(m));
        close(tty);
    }
    for (;;) pause();
}
static void init_mount(const char* fs, const char* at) {
    int rc = mount(fs, at, fs, 0, NULL);
    fprintf(stderr, "init: mount %s %s%s\n", fs, at, rc ? " FAILED" : "");
}
// the kernel's framebuffer console shares the display with the app: both
// flip the crtc. bind it only while the app is not on screen
static void init_console_bind(int on) {
    for (int i = 0; i < 4; i++) {
        char p[64], name[64] = {0};
        snprintf(p, sizeof p, "/sys/class/vtconsole/vtcon%d/name", i);
        int f = open(p, O_RDONLY);
        if (f < 0) continue;
        read(f, name, sizeof name - 1);
        close(f);
        if (!strstr(name, "frame buffer")) continue;
        snprintf(p, sizeof p, "/sys/class/vtconsole/vtcon%d/bind", i);
        f = open(p, O_WRONLY);
        if (f < 0) continue;
        write(f, on ? "1" : "0", 1);
        close(f);
        fprintf(stderr, "init: framebuffer console %s\n", on ? "bound" : "unbound");
    }
}

static void init_as_pid1(void) {
    if (getpid() != 1) return;
    fprintf(stderr, "init: orbiter launcher is pid 1\n");
    init_mount("devtmpfs", "/dev");
    init_mount("proc",     "/proc");
    init_mount("sysfs",    "/sys");
    init_mount("tmpfs",    "/tmp");
    init_mount("tmpfs",    "/run");
    setenv("HOME", "/root", 1);  // the kernel hands init HOME=/
    setenv("XDG_RUNTIME_DIR", "/run", 1);
    setenv("PATH", "/src/silver/install/bin:/usr/bin:/bin", 1);
    fprintf(stderr, "init: env HOME=%s LD_LIBRARY_PATH=%s\n",
        getenv("HOME"), getenv("LD_LIBRARY_PATH") ? getenv("LD_LIBRARY_PATH") : "(unset)");
    atexit(init_hold);
    // a heartbeat on the serial console: tells a stalled guest from a
    // stalled app when the log goes quiet
    if (fork() == 0) {
        for (int t = 5;; t += 5) { sleep(5); fprintf(stderr, "init: alive %ds\n", t); }
    }
    init_console_bind(0);
    fprintf(stderr, "init: loading the app\n");
}
#else
static void init_as_pid1(void) {}
#endif

#if defined(__linux__)
#include <linux/capability.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <linux/audit.h>
#include <sched.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <glob.h>
// the app's root, in its holder's namespace only
#define VIEW_ROOT "/mnt"

static pid_t g_view_app = 0;    // the app, as the host sees it
static const char* view_app_name = "";
static int g_view_lock = -1;    // held by a view's pid 1 while it lives

// the view an app's first instance made, if it still runs: its pid 1
// as the host sees it, checked by its process space
static pid_t view_running(const char* vf) {
    FILE* f = fopen(vf, "rb");
    if (!f) return 0;
    int p = 0;
    unsigned long ino = 0;
    int ok = fscanf(f, "%d %lu", &p, &ino) == 2;
    fclose(f);
    char ns[64];
    struct stat st;
    snprintf(ns, sizeof(ns), "/proc/%d/ns/pid", p);
    if (!ok || p <= 1 || stat(ns, &st) != 0 || st.st_ino != ino) return 0;
    return p;
}

// the /tmp folders of an app's views no pid 1 holds any more
static void view_tidy(const char* app) {
    char dir[4300], p[4600], cmd[4700];
    snprintf(dir, sizeof(dir), "%s/install/tmp/view", SILVER_ROOT);
    DIR* d = opendir(dir);
    if (!d) return;
    size_t n = strlen(app);
    struct dirent* e;
    while ((e = readdir(d))) {
        size_t len = strlen(e->d_name);
        if (strncmp(e->d_name, app, n) != 0 || e->d_name[n] != '-' ||
            len < 5 || strcmp(e->d_name + len - 5, ".lock") != 0)
            continue;
        snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
        int lf = open(p, O_RDONLY | O_CLOEXEC);
        int gone = lf >= 0 && flock(lf, LOCK_EX | LOCK_NB) == 0;
        if (lf >= 0) close(lf);
        if (!gone) continue;
        snprintf(cmd, sizeof(cmd), "rm -rf '%s/%.*s' '%s'", dir,
            (int)(len - 5), e->d_name, p);
        (void)system(cmd);
    }
    closedir(d);
}

// pid 1's own space: any process in it but itself
static int view_others(void) {
    DIR* d = opendir("/proc");
    if (!d) return 0;
    struct dirent* e;
    int n = 0;
    while (!n && (e = readdir(d)))
        n = atoi(e->d_name) > 1;
    closedir(d);
    return n;
}

static int view_mkdirs(const char* p, mode_t m) {
    char b[4300];
    snprintf(b, sizeof(b), "%s", p);
    for (char* s = b + 1; *s; s++)
        if (*s == '/') { *s = 0; mkdir(b, m); *s = '/'; }
    return mkdir(b, m) == 0 || errno == EEXIST ? 0 : -1;
}

// an empty file to mount a file on, its folders made
static void view_touch(const char* p) {
    char b[4300];
    snprintf(b, sizeof(b), "%s", p);
    char* sl = strrchr(b, '/');
    if (sl) { *sl = 0; view_mkdirs(b, 0755); }
    int fd = open(p, O_WRONLY | O_CREAT | O_CLOEXEC, 0644);
    if (fd >= 0) close(fd);
}

static int bound_write(const char* p, const char* s) {
    int fd = open(p, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    ssize_t n = write(fd, s, strlen(s));
    close(fd);
    return n == (ssize_t)strlen(s) ? 0 : -1;
}

static void view_attr(const char* dst, uint64_t set) {
    struct mount_attr ma = { .attr_set = set };
    mount_setattr(AT_FDCWD, dst, AT_RECURSIVE, &ma, sizeof(ma));
}

// src at dst in the view; a link stays a link
static int view_put(const char* src, const char* dst, int rw) {
    char at[4300];
    struct stat st;
    snprintf(at, sizeof(at), VIEW_ROOT "%s", dst);
    if (lstat(src, &st) != 0) return -1;
    if (S_ISLNK(st.st_mode)) {
        char to[4096];
        ssize_t n = readlink(src, to, sizeof(to) - 1);
        if (n < 0) return -1;
        to[n] = 0;
        view_touch(at);
        unlink(at);
        return symlink(to, at);
    }
    if (S_ISDIR(st.st_mode)) view_mkdirs(at, 0755);
    else view_touch(at);
    if (mount(src, at, NULL, MS_BIND | MS_REC, NULL) != 0) {
        fprintf(stderr, "view: bind %s: %s\n", src, strerror(errno));
        return -1;
    }
    view_attr(at, (rw ? 0 : MOUNT_ATTR_RDONLY) | MOUNT_ATTR_NOSUID);
    return 0;
}

static int view_tmpfs(const char* dst, const char* mode) {
    char at[4300];
    snprintf(at, sizeof(at), VIEW_ROOT "%s", dst);
    view_mkdirs(at, 0755);
    return mount("tmpfs", at, "tmpfs", MS_NOSUID | MS_NODEV, mode);
}

// a share, made read only before it reaches the view
static int view_graft(const char* src, const char* at, int dir, int rw) {
    unsigned fl = OPEN_TREE_CLONE | OPEN_TREE_CLOEXEC |
        (dir ? AT_RECURSIVE : 0);
    int fd = open_tree(AT_FDCWD, src, fl);
    if (fd < 0) {
        fprintf(stderr, "view: share %s: %s\n", src, strerror(errno));
        return -1;
    }
    struct mount_attr ma = {
        .attr_set = (rw ? 0 : MOUNT_ATTR_RDONLY) | MOUNT_ATTR_NOSUID };
    mount_setattr(fd, "", AT_EMPTY_PATH | (dir ? AT_RECURSIVE : 0), &ma,
        sizeof(ma));
    unlink(at);
    rmdir(at);
    if (dir) view_mkdirs(at, 0755); else view_touch(at);
    int r = move_mount(fd, "", AT_FDCWD, at, MOVE_MOUNT_F_EMPTY_PATH);
    if (r != 0)
        fprintf(stderr, "view: share %s: %s\n", src, strerror(errno));
    close(fd);
    return r;
}

// its own home: its old folders come over as hard links
static void view_home(const char* app, const char* home) {
    const char* real = getenv("HOME");
    const char* subs[] = { ".cache", ".local/state", ".config",
                           ".local/share" };
    char from[4600], to[4600], cmd[9400];
    for (int i = 0; i < 4; i++) {
        snprintf(to, sizeof(to), "%s/%s", home, subs[i]);
        view_mkdirs(to, 0755);
        for (int s = 0; s < 2; s++) {
            const char* pre = s ? "silver-" : "";
            snprintf(to, sizeof(to), "%s/%s/%s%s", home, subs[i], pre,
                app);
            if (access(to, F_OK) == 0) continue;
            snprintf(from, sizeof(from), "%s/install/home/%s/%s%s",
                SILVER_ROOT, subs[i], pre, app);
            if (access(from, F_OK) != 0 && real)
                snprintf(from, sizeof(from), "%s/%s/%s%s", real, subs[i],
                    pre, app);
            if (access(from, F_OK) != 0) continue;
            snprintf(cmd, sizeof(cmd), "cp -al '%s' '%s'", from, to);
            if (system(cmd) == 0)
                fprintf(stderr, "silver-host: %s brought into %s\n", from,
                    to);
        }
    }
}

// the holder makes the process space its children live in
static int view_ns_differs(pid_t p) {
    char a[64], b[64];
    struct stat sa, sb;
    snprintf(a, sizeof(a), "/proc/%d/ns/pid", (int)p);
    snprintf(b, sizeof(b), "/proc/%d/ns/pid_for_children", (int)p);
    return stat(a, &sa) == 0 && stat(b, &sb) == 0 && sa.st_ino != sb.st_ino;
}

// a bound app's holder: the pid given, or two parents up
static pid_t view_holder(pid_t p) {
    for (int i = 0; i < 3 && p > 1; i++) {
        if (view_ns_differs(p)) return p;
        char b[600], e[64];
        snprintf(e, sizeof(e), "/proc/%d/stat", (int)p);
        FILE* f = fopen(e, "rb");
        if (!f) return 0;
        size_t n = fread(b, 1, sizeof(b) - 1, f);
        fclose(f);
        b[n] = 0;
        char* rp = strrchr(b, ')');
        int pp = 0;
        if (!rp || sscanf(rp + 2, "%*c %d", &pp) != 1) return 0;
        p = pp;
    }
    return 0;
}

// --view-share <pid> <app> <file> [rw]; --view-unshare <path>
static int view_share(int argc, char** argv) {
    int take = strcmp(argv[1], "--view-share") == 0;
    if (argc < 5) {
        fprintf(stderr, "%s <pid> <app> <file|name>\n", argv[1]);
        return 2;
    }
    char src[4096] = "", at[4600], ns[64];
    struct stat st;
    if (take && (!realpath(argv[4], src) || stat(src, &st) != 0)) {
        fprintf(stderr, "view: no file %s\n", argv[4]);
        return 1;
    }
    pid_t hold = view_holder(atoi(argv[2]));
    if (!hold) {
        fprintf(stderr, "view: %s is not a bound app\n", argv[2]);
        return 1;
    }
    // a share is at its own path in the view
    if (!take) snprintf(src, sizeof(src), "%s", argv[4]);
    snprintf(at, sizeof(at), VIEW_ROOT "%s", src);
    snprintf(ns, sizeof(ns), "/proc/%d/ns/user", (int)hold);
    int userns = open(ns, O_RDONLY | O_CLOEXEC);
    snprintf(ns, sizeof(ns), "/proc/%d/ns/mnt", (int)hold);
    int mntns = open(ns, O_RDONLY | O_CLOEXEC);
    if (userns < 0 || mntns < 0 || setns(userns, CLONE_NEWUSER) != 0 ||
        setns(mntns, CLONE_NEWNS) != 0) {
        fprintf(stderr, "view: cannot enter %s's view: %s\n", argv[3],
            strerror(errno));
        return 1;
    }
    // a joined instance's holder is inside the view itself
    char seen[4600];
    snprintf(seen, sizeof(seen), VIEW_ROOT "%s", SILVER_ROOT);
    if (access(seen, F_OK) != 0) snprintf(at, sizeof(at), "%s", src);
    umount2(at, MNT_DETACH);
    if (!take) {
        unlink(at);
        rmdir(at);
        return 0;
    }
    int rw = argc > 5 && strcmp(argv[5], "rw") == 0;
    if (view_graft(src, at, S_ISDIR(st.st_mode), rw) != 0) return 1;
    printf("%s\n", src);
    return 0;
}

static int bound_under(const char* p, const char* dir) {
    size_t n = strlen(dir);
    return strncmp(p, dir, n) == 0 && (p[n] == '/' || p[n] == 0);
}

// the view shows it as it is on the host
static int bound_reaches(const char* p) {
    const char* own[] = { "/install/home", "/install/tmp" };
    char d[4300];
    for (size_t i = 0; i < sizeof(own) / sizeof(*own); i++) {
        snprintf(d, sizeof(d), "%s%s", SILVER_ROOT, own[i]);
        if (bound_under(p, d)) return 0;
    }
    const char* seen[] = { SILVER_ROOT, "/usr", "/etc", "/opt" };
    for (size_t i = 0; i < sizeof(seen) / sizeof(*seen); i++)
        if (bound_under(p, seen[i])) return 1;
    return 0;
}

// the app's root: read only but its own folders
static int view_build(const char* home, const char* run,
                      const char* tmp, const char* log) {
    char p[4600], q[4600];
    if (mount("tmpfs", VIEW_ROOT, "tmpfs", MS_NOSUID | MS_NODEV,
              "mode=755") != 0) {
        fprintf(stderr, "view: root: %s\n", strerror(errno));
        return -1;
    }
    mount(NULL, VIEW_ROOT, NULL, MS_PRIVATE, NULL);
    const char* sys[] = { "/usr", "/etc", "/opt", "/bin", "/sbin", "/lib",
        "/lib32", "/lib64", "/libx32", "/sys", "/var/cache/fontconfig",
        "/run/systemd/resolve", "/tmp/.X11-unix" };
    // the standard places, each the app's own folder outside
    uid_t uid = getuid();
    const char* real = getenv("HOME");
    // home first: a program naming a disk by its first mount (Steam's
    // library drive) names it by the home, never /tmp
    if (real && *real) view_put(home, real, 1);
    view_put(tmp, "/tmp", 1);
    view_tmpfs("/run", "mode=755");
    snprintf(q, sizeof(q), "/run/user/%u", uid);
    view_put(run, q, 1);
    for (size_t i = 0; i < sizeof(sys) / sizeof(*sys); i++)
        view_put(sys[i], sys[i], 0);
    // the devices a drawing, sounding app opens; its own ptys
    view_tmpfs("/dev", "mode=755");
    const char* dev[] = { "/dev/null", "/dev/zero", "/dev/full",
        "/dev/random", "/dev/urandom", "/dev/tty", "/dev/dri",
        "/dev/snd", "/dev/kfd", "/dev/nvidia-caps" };
    for (size_t i = 0; i < sizeof(dev) / sizeof(*dev); i++)
        if (access(dev[i], F_OK) == 0) view_put(dev[i], dev[i], 1);
    glob_t g;
    if (glob("/dev/nvidia*", 0, NULL, &g) == 0) {
        for (size_t i = 0; i < g.gl_pathc; i++)
            if (strcmp(g.gl_pathv[i], "/dev/nvidia-caps") != 0)
                view_put(g.gl_pathv[i], g.gl_pathv[i], 1);
        globfree(&g);
    }
    view_tmpfs("/dev/shm", "mode=1777");
    view_mkdirs(VIEW_ROOT "/dev/pts", 0755);
    mount("devpts", VIEW_ROOT "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC,
        "newinstance,ptmxmode=0666,mode=620");
    symlink("pts/ptmx", VIEW_ROOT "/dev/ptmx");
    symlink("/proc/self/fd", VIEW_ROOT "/dev/fd");
    symlink("/proc/self/fd/0", VIEW_ROOT "/dev/stdin");
    symlink("/proc/self/fd/1", VIEW_ROOT "/dev/stdout");
    symlink("/proc/self/fd/2", VIEW_ROOT "/dev/stderr");
    view_mkdirs(VIEW_ROOT "/proc", 0555);
    // silver's folder, but no app's home or tmp; its own log
    view_put(SILVER_ROOT, SILVER_ROOT, 0);
    snprintf(p, sizeof(p), "%s/install/home", SILVER_ROOT);
    view_tmpfs(p, "mode=755");
    snprintf(p, sizeof(p), "%s/install/tmp", SILVER_ROOT);
    view_tmpfs(p, "mode=755");
    view_put(log, log, 1);
    // each slot's log: a later instance of the app joins this view
    for (int k = 1; k < HOST_APPS; k++) {
        app_log_path(q, sizeof(q), view_app_name, k);
        if (strcmp(q, log) == 0) continue;
        view_touch(q);
        view_put(q, q, 1);
    }
    // the covers themselves take no files
    struct mount_attr ro = { .attr_set = MOUNT_ATTR_RDONLY };
    snprintf(q, sizeof(q), VIEW_ROOT "%s/install/tmp", SILVER_ROOT);
    mount_setattr(AT_FDCWD, q, 0, &ro, sizeof(ro));
    snprintf(q, sizeof(q), VIEW_ROOT "%s/install/home", SILVER_ROOT);
    mount_setattr(AT_FDCWD, q, 0, &ro, sizeof(ro));
    // the sound servers' sockets in its runtime folder
    const char* rt = getenv("XDG_RUNTIME_DIR");
    const char* snd[] = { "pulse/native", "pipewire-0" };
    for (size_t i = 0; rt && i < sizeof(snd) / sizeof(*snd); i++) {
        snprintf(q, sizeof(q), "%s/%s", rt, snd[i]);
        if (!realpath(q, p)) continue;
        snprintf(q, sizeof(q), "/run/user/%u/%s", uid, snd[i]);
        view_put(p, q, 0);
    }
    const char* xa = getenv("XAUTHORITY");
    if (xa && *xa && realpath(xa, p)) view_put(p, xa, 0);
    return 0;
}

#define VF_STMT(c, k)        f[n++] = (struct sock_filter)BPF_STMT(c, k)
#define VF_JUMP(c, k, t, e)  f[n++] = (struct sock_filter)BPF_JUMP(c, k, t, e)
#define VF_RET(v)            VF_STMT(BPF_RET | BPF_K, v)

// one architecture's refusals; its own namespaces are allowed
static int view_rules(struct sock_filter* f, int n, const int* deny,
                      size_t nd, int ioctl_nr) {
    unsigned perm = SECCOMP_RET_ERRNO | EPERM;
    VF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr));
    for (size_t i = 0; i < nd; i++) {
        VF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, deny[i], 0, 1);
        VF_RET(perm);
    }
    VF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ioctl_nr, 0, 5);
    VF_STMT(BPF_LD | BPF_W | BPF_ABS,
        offsetof(struct seccomp_data, args) + sizeof(uint64_t));
    VF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, TIOCSTI, 1, 0);
    VF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, TIOCLINUX, 0, 1);
    VF_RET(perm);
    VF_RET(SECCOMP_RET_ALLOW);
    VF_RET(SECCOMP_RET_ALLOW);
    return n;
}

// calls refused: other processes, mounts, kernel powers
static int view_filter(void) {
    struct sock_filter f[400];
    int n = 0;
    const int deny[] = {
        __NR_ptrace, __NR_process_vm_readv, __NR_process_vm_writev,
        __NR_pidfd_getfd, __NR_kexec_load, __NR_kexec_file_load,
        __NR_init_module, __NR_finit_module, __NR_delete_module, __NR_bpf,
        __NR_perf_event_open, __NR_keyctl, __NR_add_key, __NR_request_key,
        __NR_swapon, __NR_swapoff, __NR_reboot,
        __NR_settimeofday, __NR_clock_settime, __NR_clock_adjtime,
        __NR_acct, __NR_quotactl, __NR_name_to_handle_at,
        __NR_open_by_handle_at, __NR_userfaultfd, __NR_vhangup,
        __NR_io_uring_setup, __NR_io_uring_enter, __NR_io_uring_register,
        __NR_syslog, __NR_fanotify_init,
#if defined(__x86_64__)
        __NR_iopl, __NR_ioperm, __NR_uselib, __NR_ustat, __NR_sysfs,
#endif
    };
    size_t nd = sizeof(deny) / sizeof(*deny);
    VF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, arch));
#if defined(__x86_64__)
    // the same refusals by 32-bit x86's numbers (unistd_32.h)
    const int deny32[] = { 26, 347, 348, 438, 283, 128, 350, 129, 357,
        336, 288, 286, 287, 87, 115, 88, 79, 25, 124, 264, 404, 343, 405,
        51, 131, 341, 342, 374, 111, 425, 426, 427, 103, 338, 110, 101,
        86, 62, 135, 166, 113 };
    VF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_I386, 0, 1);
    int to32 = n;
    VF_STMT(BPF_JMP | BPF_JA, 0);
    VF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 1, 0);
    VF_RET(SECCOMP_RET_KILL_PROCESS);
    VF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr));
    // x32 numbers reach the same calls by another door
    VF_JUMP(BPF_JMP | BPF_JSET | BPF_K, 0x40000000, 0, 1);
    VF_RET(SECCOMP_RET_ERRNO | EPERM);
    n = view_rules(f, n, deny, nd, __NR_ioctl);
    f[to32].k = (unsigned)(n - to32 - 1);
    n = view_rules(f, n, deny32, sizeof(deny32) / sizeof(*deny32), 54);
#elif defined(__aarch64__)
    VF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_AARCH64, 1, 0);
    VF_RET(SECCOMP_RET_KILL_PROCESS);
    n = view_rules(f, n, deny, nd, __NR_ioctl);
#endif
    struct sock_fprog prog = { .len = (unsigned short)n, .filter = f };
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) return -1;
    return prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
}
#undef VF_RET
#undef VF_JUMP
#undef VF_STMT

static void view_signal(int sig) {
    if (g_view_app <= 0) return;
    kill(g_view_app, sig);
    // an app parked in a stop takes it as it wakes
    if (sig != SIGUSR1 && sig != SIGUSR2) kill(g_view_app, SIGCONT);
}

static int broker_line(int fd, char* out, size_t cap) {
    size_t n = 0;
    while (n + 1 < cap) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return -1;
        if (c == '\n') break;
        out[n++] = c;
    }
    out[n] = 0;
    return (int)n;
}

// the holder: the app's host pid, its builds, its end
static void view_hold(const char* app, int fd, int status_fd, pid_t init,
                      const char* sock, const char* run) {
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &one, sizeof(one));
    pid_t build = 0;
    char in[8192];
    size_t got = 0;
    int st = 0, ended = 0;
    while (!ended) {
        struct pollfd sf = { .fd = status_fd, .events = POLLIN };
        if (poll(&sf, 1, 0) > 0 && (sf.revents & POLLIN) &&
            read(status_fd, &st, sizeof(st)) == sizeof(st)) {
            ended = 1;
            break;
        }
        struct pollfd pf = { .fd = fd, .events = POLLIN };
        int pr = poll(&pf, fd >= 0 ? 1 : 0, 50);
        if (pr > 0 && (pf.revents & (POLLIN | POLLHUP))) {
            char cb[CMSG_SPACE(sizeof(struct ucred))];
            struct iovec io = { in + got, sizeof(in) - 1 - got };
            struct msghdr mh = { .msg_iov = &io, .msg_iovlen = 1,
                .msg_control = cb, .msg_controllen = sizeof(cb) };
            ssize_t r = recvmsg(fd, &mh, 0);
            if (r <= 0) { close(fd); fd = -1; }
            struct cmsghdr* cm = r > 0 ? CMSG_FIRSTHDR(&mh) : NULL;
            if (cm && cm->cmsg_type == SCM_CREDENTIALS) {
                struct ucred uc;
                memcpy(&uc, CMSG_DATA(cm), sizeof(uc));
                g_view_app = uc.pid;
            }
            got += r > 0 ? (size_t)r : 0;
            in[got] = 0;
            char* nl;
            while (fd >= 0 && (nl = strchr(in, '\n'))) {
                *nl = 0;
                char out[64] = "";
                int clean = 0;
                if (strcmp(in, "pid") == 0)
                    snprintf(out, sizeof(out), "pid %d\n", (int)g_view_app);
                // its own module only: never a path the app names
                else if (sscanf(in, "build %d", &clean) == 1 && !build) {
                    build = rebuild_spawn(app, clean != 0);
                    if (build < 0) {
                        build = 0;
                        snprintf(out, sizeof(out), "built %d\n", 1 << 8);
                    }
                }
                if (out[0]) (void)!write(fd, out, strlen(out));
                size_t used = (size_t)(nl - in) + 1;
                memmove(in, nl + 1, got - used + 1);
                got -= used;
            }
        }
        int ws;
        pid_t w;
        while ((w = waitpid(-1, &ws, WNOHANG)) > 0) {
            if (w == build) {
                char out[64];
                snprintf(out, sizeof(out), "built %d\n", ws);
                if (fd >= 0) (void)!write(fd, out, strlen(out));
                build = 0;
            } else if (w == init) {
                ended = 1;
                if (read(status_fd, &st, sizeof(st)) != sizeof(st))
                    st = ws;
            }
        }
    }
    // the host's name for its socket, and its runtime folder
    char to[4600];
    ssize_t n = readlink(sock, to, sizeof(to) - 1);
    if (n > 0) { to[n] = 0; if (strstr(to, "silver-view/")) unlink(sock); }
    (void)run;
    if (WIFSIGNALED(st)) {
        signal(WTERMSIG(st), SIG_DFL);
        kill(getpid(), WTERMSIG(st));
        pause();
    }
    _exit(WIFEXITED(st) ? WEXITSTATUS(st) : 1);
}

// the app's env: no tokens, agents or buses of the user
static void view_env(void) {
    const char* keep[] = { "DISPLAY", "XAUTHORITY", "PATH", "TERM",
        "COLORTERM", "LANG", "LANGUAGE", "TZ", "USER", "LOGNAME", "SHELL",
        "LD_LIBRARY_PATH", "XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP",
        "LC_", "SILVER", "VK_", "VULKAN_", "__GL_", "__NV_", "__VK_",
        "MESA_", "NVIDIA_", "PULSE_", "PIPEWIRE_", "ALSA_", "FONTCONFIG_",
        "XCURSOR_", "GDK_SCALE", "QT_SCALE", "AU_", "ORBITER_", "HOME",
        "XDG_CACHE_HOME", "XDG_STATE_HOME", "XDG_CONFIG_HOME",
        "XDG_DATA_HOME", "XDG_RUNTIME_DIR", "XDG_DATA_DIRS",
        // where it was started: an agent's shell, and its session
        "AI_AGENT", "CLAUDE_CODE_SESSION_ID" };
    for (int i = 0; environ[i];) {
        const char* e = environ[i];
        const char* eq = strchr(e, '=');
        size_t n = eq ? (size_t)(eq - e) : strlen(e);
        int ok = 0;
        for (size_t k = 0; k < sizeof(keep) / sizeof(*keep) && !ok; k++) {
            size_t kl = strlen(keep[k]);
            int pre = keep[k][kl - 1] == '_';
            ok = pre ? n >= kl && strncmp(e, keep[k], kl) == 0
                     : n == kl && strncmp(e, keep[k], kl) == 0;
            // silver's own: SILVER, SILVER_*
            if (!ok && kl == 6 && strncmp(keep[k], "SILVER", 6) == 0)
                ok = strncmp(e, "SILVER_", 7) == 0;
        }
        if (ok) { i++; continue; }
        char name[256];
        snprintf(name, sizeof(name), "%.*s", (int)n, e);
        unsetenv(name);
    }
}

// pid 1: enters the root, waits on the app, reports
static void view_init(int status_fd, int broker_fd) {
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    if (unshare(CLONE_NEWNS) != 0 ||
        mount(NULL, "/", NULL, MS_REC | MS_SLAVE, NULL) != 0 ||
        chdir(VIEW_ROOT) != 0) {
        fprintf(stderr, "view: enter: %s\n", strerror(errno));
        _exit(127);
    }
    mkdir(".old", 0700);
    if (syscall(SYS_pivot_root, ".", ".old") != 0) {
        fprintf(stderr, "view: pivot: %s\n", strerror(errno));
        _exit(127);
    }
    if (chdir("/") != 0 ||
        mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC,
              NULL) != 0) {
        fprintf(stderr, "view: proc: %s\n", strerror(errno));
        _exit(127);
    }
    umount2("/.old", MNT_DETACH);
    rmdir("/.old");
    pid_t a = fork();
    if (a < 0) _exit(127);
    if (a == 0) {
        close(status_fd);
        if (g_view_lock >= 0) close(g_view_lock);
        return;
    }
    close(broker_fd);
    for (int f = 0; f < 4096; f++)
        if (f != status_fd && f != 2 && f != g_view_lock) close(f);
    int st = 0;
    for (;;) {
        int ws;
        pid_t w = waitpid(-1, &ws, 0);
        if (w < 0 && errno == ECHILD) break;
        if (w == a) { st = ws; break; }
    }
    // later instances still in it: the view stays for them
    if (view_others()) prctl(PR_SET_PDEATHSIG, 0);
    (void)!write(status_fd, &st, sizeof(st));
    close(status_fd);
    while (view_others()) {
        while (waitpid(-1, NULL, WNOHANG) > 0) {}
        usleep(200000);
    }
    _exit(0);
}

// a later instance: into the first one's view, its app a child here
static void view_join(pid_t keep, int status_fd, int broker_fd) {
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    const char* kinds[] = { "user", "mnt", "pid" };
    const int flags[] = { CLONE_NEWUSER, CLONE_NEWNS, CLONE_NEWPID };
    int fds[3];
    char ns[64];
    for (int i = 0; i < 3; i++) {
        snprintf(ns, sizeof(ns), "/proc/%d/ns/%s", (int)keep, kinds[i]);
        fds[i] = open(ns, O_RDONLY | O_CLOEXEC);
    }
    for (int i = 0; i < 3; i++)
        if (fds[i] < 0 || setns(fds[i], flags[i]) != 0) {
            fprintf(stderr, "view: join %s: %s\n", kinds[i],
                strerror(errno));
            _exit(127);
        }
    if (chdir("/") != 0) _exit(127);
    pid_t a = fork();
    if (a < 0) _exit(127);
    if (a == 0) { close(status_fd); return; }
    close(broker_fd);
    int ws = 0;
    while (waitpid(a, &ws, 0) < 0 && errno == EINTR) {}
    (void)!write(status_fd, &ws, sizeof(ws));
    _exit(0);
}

// an app with no view does not run
static void bound_lock(const char* app, int argc, char** argv) {
    uid_t uid = getuid();
    gid_t gid = getgid();
    int slot = atoi(getenv("SILVER_APP_SLOT"));
    // ubuntu lets a namespace be made but strips it of its powers
    // unless our apparmor profile names this program
    char label[256] = "";
    int lf = open("/proc/self/attr/current", O_RDONLY | O_CLOEXEC);
    if (lf >= 0) { (void)!read(lf, label, sizeof(label) - 1); close(lf); }
    int fenced = 0;
    lf = open("/proc/sys/kernel/apparmor_restrict_unprivileged_userns",
        O_RDONLY | O_CLOEXEC);
    if (lf >= 0) { char c = 0; (void)!read(lf, &c, 1); close(lf);
                   fenced = c == '1'; }
    if (fenced && strncmp(label, "silver-host", 11) != 0) {
        fprintf(stderr, "silver-host: no private view for %s: install "
            "support/apparmor/silver-host\n", app);
        exit(1);
    }
    // its folders on the host: home, tmp, runtime, the share point;
    // one view an app: a later instance joins the first one's
    view_app_name = app;
    char home[4300], tmp[4300], run[4300], sock[4300], log[4300];
    const char* rt = getenv("XDG_RUNTIME_DIR");
    char rtd[512];
    snprintf(rtd, sizeof(rtd), "/run/user/%u", uid);
    if (!rt || !*rt) rt = rtd;
    char vf[4400];
    snprintf(vf, sizeof(vf), "%s/silver-view/%s.view", rt, app);
    pid_t keep = view_running(vf);
    snprintf(home, sizeof(home), "%s/install/home/%s", SILVER_ROOT, app);
    view_home(app, home);
    // a view's /tmp: its first holder's, empty as a boot leaves one;
    // those of views gone are let go
    snprintf(tmp, sizeof(tmp), "%s/install/tmp/view/%s-%d", SILVER_ROOT,
        app, (int)getpid());
    snprintf(run, sizeof(run), "%s/silver-view/%s", rt, app);
    if (!keep) {
        snprintf(sock, sizeof(sock), "rm -rf '%s'", run);
        (void)system(sock);
        view_tidy(app);
        snprintf(sock, sizeof(sock), "%s.lock", tmp);
        g_view_lock = open(sock, O_RDONLY | O_CREAT, 0600);
        if (g_view_lock >= 0) flock(g_view_lock, LOCK_SH);
    }
    if (!keep) view_mkdirs(tmp, 01777);
    view_mkdirs(run, 0700);
    app_log_path(log, sizeof(log), app, slot);
    view_touch(log);
    // the host finds its socket where it always was
    snprintf(sock, sizeof(sock), "%s/trinity-%s-%d.sock", rt, app, slot);
    unlink(sock);
    char rel[600];
    snprintf(rel, sizeof(rel), "silver-view/%s/trinity-%s-%d.sock", app,
        app, slot);
    (void)!symlink(rel, sock);
    const int fwd[] = { SIGTERM, SIGINT, SIGHUP, SIGQUIT, SIGUSR1,
                        SIGUSR2 };
    int bp[2], sp[2];
    if (keep) {
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, bp) != 0 ||
            pipe2(sp, O_CLOEXEC) != 0) exit(1);
        for (size_t i = 0; i < sizeof(fwd) / sizeof(*fwd); i++)
            signal(fwd[i], view_signal);
        pid_t c = fork();
        if (c < 0) exit(1);
        if (c > 0) {
            close(bp[1]);
            close(sp[1]);
            fprintf(stderr, "silver-host: %s joins its view\n", app);
            view_hold(app, bp[0], sp[0], c, sock, run);
        }
        close(bp[0]);
        close(sp[0]);
        view_join(keep, sp[1], bp[1]);
    } else {
        // the launch line's paths, found while the host's tree is seen
        char** found = calloc(argc, sizeof(char*));
        char real[4096];
        for (int i = 1; i < argc; i++)
            if (argv[i][0] == '/' && realpath(argv[i], real) &&
                strcmp(real, "/") != 0 && !bound_reaches(real))
                found[i] = strdup(real);
        if (unshare(CLONE_NEWUSER | CLONE_NEWNS) != 0) {
            fprintf(stderr, "silver-host: no private view for %s: %s\n", app,
                strerror(errno));
            exit(1);
        }
        char map[64];
        bound_write("/proc/self/setgroups", "deny");
        snprintf(map, sizeof(map), "%u %u 1", uid, uid);
        int mapped = bound_write("/proc/self/uid_map", map) == 0;
        snprintf(map, sizeof(map), "%u %u 1", gid, gid);
        mapped = mapped && bound_write("/proc/self/gid_map", map) == 0;
        // the host's new mounts still arrive; ours stay ours
        if (!mapped || mount(NULL, "/", NULL, MS_REC | MS_SLAVE, NULL) != 0 ||
            view_build(home, run, tmp, log) != 0) {
            fprintf(stderr, "silver-host: %s's view failed: %s\n", app,
                strerror(errno));
            exit(1);
        }
        // a launch path it cannot reach is shared there, read only
        for (int i = 1; i < argc; i++) {
            if (!found[i]) continue;
            struct stat st;
            char at[4600];
            snprintf(at, sizeof(at), VIEW_ROOT "%s", found[i]);
            if (stat(found[i], &st) == 0 &&
                view_graft(found[i], at, S_ISDIR(st.st_mode), 0) == 0)
                fprintf(stderr, "silver-host: %s shared into %s's view\n",
                    found[i], app);
            free(found[i]);
        }
        free(found);
        // orbiter's later shares reach the app through this
        mount(NULL, VIEW_ROOT, NULL, MS_REC | MS_SHARED, NULL);
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, bp) != 0 ||
            pipe2(sp, O_CLOEXEC) != 0 || unshare(CLONE_NEWPID) != 0) {
            fprintf(stderr, "silver-host: %s's process space: %s\n", app,
                strerror(errno));
            exit(1);
        }
        for (size_t i = 0; i < sizeof(fwd) / sizeof(*fwd); i++)
            signal(fwd[i], view_signal);
        pid_t init = fork();
        if (init < 0) exit(1);
        if (init > 0) {
            close(bp[1]);
            close(sp[1]);
            // the lock is pid 1's now; later instances find it by pid 1
        if (g_view_lock >= 0) close(g_view_lock);
            char ns[64];
            struct stat pst;
            snprintf(ns, sizeof(ns), "/proc/%d/ns/pid", (int)init);
            FILE* vw = stat(ns, &pst) == 0 ? fopen(vf, "wb") : NULL;
            if (vw) {
                fprintf(vw, "%d %lu\n", (int)init, (unsigned long)pst.st_ino);
                fclose(vw);
            }
            fprintf(stderr, "silver-host: %s has its own root\n", app);
            view_hold(app, bp[0], sp[0], init, sock, run);
        }
        close(bp[0]);
        close(sp[0]);
        view_init(sp[1], bp[1]);
    }
    // the app
    for (size_t i = 0; i < sizeof(fwd) / sizeof(*fwd); i++)
        signal(fwd[i], SIG_DFL);
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    g_broker = bp[1];
    char ans[64] = "";
    if (write(g_broker, "pid\n", 4) == 4) broker_line(g_broker, ans,
        sizeof(ans));
    int hp = 0;
    if (sscanf(ans, "pid %d", &hp) == 1 && hp > 0) {
        char hs[32];
        snprintf(hs, sizeof(hs), "%d", hp);
        setenv("SILVER_HOST_PID", hs, 1);
        // the slot names the app itself, before its first frame
        const char* fds = getenv("SILVER_SHM_FD");
        int sfd = (fds && *fds) ? atoi(fds) : -1;
        void* m = sfd < 0 ? MAP_FAILED : mmap(0, sizeof(HostShared),
            PROT_READ | PROT_WRITE, MAP_SHARED, sfd, 0);
        if (m != MAP_FAILED) {
            if (slot > 0 && slot < HOST_APPS)
                ((HostShared*)m)->app[slot].app_pid = hp;
            munmap(m, sizeof(HostShared));
        }
    }
    // the standard places: ~, /tmp and /run/user/<uid> are its own
    const char* envs[] = { "XDG_CACHE_HOME", "XDG_STATE_HOME",
        "XDG_CONFIG_HOME", "XDG_DATA_HOME", "TMPDIR" };
    for (size_t i = 0; i < sizeof(envs) / sizeof(*envs); i++)
        unsetenv(envs[i]);
    setenv("XDG_RUNTIME_DIR", rtd, 1);
    view_env();
    // no powers left: it cannot undo the root it was given
    struct __user_cap_header_struct ch = { _LINUX_CAPABILITY_VERSION_3, 0 };
    struct __user_cap_data_struct cd[2];
    memset(cd, 0, sizeof(cd));
    syscall(SYS_capset, &ch, cd);
    if (view_filter() != 0) {
        fprintf(stderr, "silver-host: %s's filter: %s\n", app,
            strerror(errno));
        exit(1);
    }
    fprintf(stderr, "silver-host: %s bound to its view\n", app);
}

// a build asked of the holder: 0 while it runs, 1 when done
static int broker_build_wait(int block, int* st) {
    struct pollfd pf = { .fd = g_broker, .events = POLLIN };
    if (!block && poll(&pf, 1, 0) <= 0) return 0;
    char ans[64];
    if (broker_line(g_broker, ans, sizeof(ans)) < 0 ||
        sscanf(ans, "built %d", st) != 1)
        *st = 1 << 8;
    return 1;
}
#endif

int main(int argc, char** argv) {
#if defined(__linux__) && defined(SILVER_ROOT)
    if (argc > 1 && (strcmp(argv[1], "--view-share") == 0 ||
                     strcmp(argv[1], "--view-unshare") == 0))
        return view_share(argc, argv);
#endif
    init_as_pid1();
#ifndef _WIN32
    // the nvidia driver opens a /dev/nvidia fd per GPU allocation: a reload's
    // init spike passes the 1024 soft limit, so run at the hard limit
    struct rlimit nofile;
    if (getrlimit(RLIMIT_NOFILE, &nofile) == 0 && nofile.rlim_cur < nofile.rlim_max) {
        nofile.rlim_cur = nofile.rlim_max;
        setrlimit(RLIMIT_NOFILE, &nofile);
    }
#endif
#ifdef _WIN32
    // the module we dlopen pulls vulkan-1, opencv, OpenEXR, libpng ... and they
    // live in install/bin. windows has no rpath, so the loader finds them only
    // through PATH -- silver.exe does the same at startup
    {
        // SILVER_ROOT is spelled with forward slashes; the loader and the
        // vulkan layer discovery both want backslashes here
        char root[1024];
        snprintf(root, sizeof(root), "%s", SILVER_ROOT);
        for (char* q = root; *q; q++) if (*q == '/') *q = '\\';

        const char* prev = getenv("PATH");
        char buf[8192];
        snprintf(buf, sizeof(buf), "%s\\install\\bin;%s\\install\\build;%s",
            root, root, prev ? prev : "");
        setenv("PATH", buf, 1);
        // VK_LAYER_PATH comes from the module's own export and nowhere else.
        // this used to fall back to a path built here, so a run whose export
        // carried the entry and one whose export had lost it got two different
        // answers -- and the second rendered black
    }
#endif

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
#ifdef __APPLE__
    {   // the kernel's answer beats argv[0], as /proc/self/exe does on linux
        char mac[4096]; uint32_t mn = sizeof(mac);
        if (_NSGetExecutablePath(mac, &mn) == 0 && realpath(mac, abspath) == NULL)
            strncpy(abspath, mac, sizeof(abspath) - 1);
    }
#endif
    strncpy(g_exe_path, abspath, sizeof(g_exe_path) - 1);
    char self[4096], self2[4096];
    strncpy(self,  abspath, sizeof(self)  - 1); self[sizeof(self)   - 1] = '\0';
    strncpy(self2, abspath, sizeof(self2) - 1); self2[sizeof(self2) - 1] = '\0';
    char* name         = basename(self);
    const char* bindir = dirname(self2);
    g_app_name = name;   // the crash handler and init's log dump name files by it
#ifdef _WIN32
    // argv[0] carries .exe here; the module and its product are named without it
    { char* dot = strrchr(name, '.');
      if (dot && strcmp(dot, ".exe") == 0) *dot = '\0'; }
#endif
    // a debug host is <name>-dbg: the module is <name>, built debug
    { size_t nl = strlen(name);
      if (nl > 4 && strcmp(name + nl - 4, "-dbg") == 0) {
          name[nl - 4] = '\0';
          setenv("SILVER_DEBUG_BUILD", "1", 1);
      } }

    // BEFORE anything that changes cwd or rebuilds: the child re-runs main from
    // the launch directory and does the whole normal startup itself. hooking in
    // later handed it the share dir as its launch cwd and made both processes
    // race the same rebuild. the supervisor blocks in supervise_wait for the
    // app's whole normal lifetime; it also spawns hosted apps into channel
    // slots on request. orbiter itself is supervised like any app — its
    // silver-host hosts its pane apps.
    if (isolate_requested(argc, argv)) {
        int exit_code = 0;
        int r = supervise_wait(argc, argv, name, bindir, &exit_code);
        if (r == 0) return exit_code;
        // r < 0: isolation unavailable — run the app in-process below
    }
    g_app_name = name;
    { const char* se = getenv("SILVER_APP_SLOT");
      g_log_slot = (se && *se) ? atoi(se) : -1; }
    // the log is named for the APP, not the root element — so the tee
    // (host_log_setup) and the crash handler agree on <logdir>/<app>.log
#ifdef SILVER_SHARE_NAME
    path_set_share_name(SILVER_SHARE_NAME);
#else
    path_set_share_name(name);
#endif
    // and the tee lives in the app, which cannot see temp_dir(): publish it
    setenv("SILVER_LOG_DIR", temp_dir(), 1);

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
        sa.sa_sigaction = crash_handler;
        sa.sa_flags     = SA_ONSTACK | SA_SIGINFO;
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


    // supervised child: map the channel so the crash handler can publish the
    // frozen verdict (state 4) before stopping at the crash site
    if (getenv(ISOLATE_CHILD_ENV) && !g_shm) {
        const char* fs = getenv("SILVER_SHM_FD");
        int shmfd = fs ? atoi(fs) : -1;
        if (shmfd >= 0) {
            void* p = mmap(0, sizeof(HostShared), PROT_READ | PROT_WRITE,
                MAP_SHARED, shmfd, 0);
            if (p != MAP_FAILED) g_shm = (HostShared*)p;
        }
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

    const char* build_name = name;
#ifdef SILVER_SHARE_NAME
    build_name = SILVER_SHARE_NAME;
#endif
    char build_key[512];
    snprintf(build_key, sizeof(build_key), "%s%s", build_name, SILVER_BUILD_TAG);
    char product[4096];
    snprintf(product, sizeof(product), "%s/%s.product", bindir,
        build_key);
    // a system install keeps nothing beside /usr/bin/<name>: the product
    // and its libs live in ../lib/<name>/
    char libdir[4096];
    snprintf(libdir, sizeof(libdir), "%s/../lib/%s", bindir, name);
    if (access(product, F_OK) != 0)
        snprintf(product, sizeof(product), "%s/%s.product", libdir, build_key);

    char artifacts[4096];
    snprintf(artifacts, sizeof(artifacts), "%s/%s.source", bindir,
        build_key);
    if (access(artifacts, F_OK) != 0)
        snprintf(artifacts, sizeof(artifacts), "%s/%s.source", libdir, build_key);

    // record the launch cwd before we cd to the share, so the app can resolve its
    // config (e.g. orbiter.agi) against where it was started, not the share dir.
    char launch_cwd[4096];
    if (getcwd(launch_cwd, sizeof(launch_cwd))) {
        setenv("SILVER_STARTUP", launch_cwd, 1);
    }
#ifdef SILVER_ROOT
    // apps resolve {SILVER}/export and root modules through this
    setenv("SILVER", SILVER_ROOT, 1);
#endif
#if defined(__linux__) && defined(SILVER_ROOT)
    // a slot is an app orbiter runs; orbiter itself is the user
    if (getenv("SILVER_APP_SLOT")) bound_lock(name, argc, argv);
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
    // a relative link resolves beside the product file, not the cwd (share).
    // a drive letter and a UNC root are absolute too: testing only for '/'
    // joined "C:/..." onto the product dir and dlopen got a doubled path
    bool lib_abs = lib[0] == '/' || lib[0] == '\\' ||
        (((lib[0] >= 'A' && lib[0] <= 'Z') || (lib[0] >= 'a' && lib[0] <= 'z'))
            && lib[1] == ':');
    if (!lib_abs) {
        char pdir[4096], rel[4096];
        strncpy(pdir, product, sizeof(pdir) - 1); pdir[sizeof(pdir) - 1] = '\0';
        snprintf(rel, sizeof(rel), "%s/%s", dirname(pdir), lib);
        strncpy(lib, rel, sizeof(lib) - 1); lib[sizeof(lib) - 1] = '\0';
    }

#ifdef _WIN32
    // a COPY from the very first load: windows cannot relink a dll this
    // process has mapped, so holding the product itself makes the NEXT build
    // fail -- which is exactly what live reload depends on being able to do
    void* handle = reload_dlopen(lib, file_mtime(product));
#else
    void* handle = try_dlopen(lib);
#endif
    if (!handle) { fprintf(stderr, "%s: dlopen %s: %s\n", name, lib, dlerror()); return 1; }

    // the recording's mic is the host's, so a reload keeps it
    au_live_mic_hooks_fn mic_hooks = (au_live_mic_hooks_fn)dlsym(RTLD_DEFAULT, "au_live_mic_hooks");
    if (mic_hooks) mic_hooks(host_mic_start, host_mic_read);

    // initial startup: call silver_live_init explicitly (not a global constructor)
    init_fn    do_init    = dlsym(handle, INIT_SYM);
    frame_fn   do_frame   = dlsym(handle, FRAME_SYM);
    destroy_fn do_destroy = dlsym(handle, DESTROY_SYM);
    stash_args(handle, argc, argv);
    // debug launch: park HERE, before any app code, so orbiter can attach lldb and
    // arm breakpoints (including in init) before continuing us (SIGCONT).
    if (getenv("SILVER_DEBUG")) {
        // orbiter is a sibling, not our ancestor — yama scope 1 blocks it
#ifdef __linux__
        prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
#endif
        fprintf(stderr, "silver-host: SILVER_DEBUG — pid %d stopped before init for attach\n", (int)getpid());
        raise(SIGSTOP);
    }
    if (do_init) do_init();
    {
        char app_lib[4096];
        ssize_t al = readlink(product, app_lib, sizeof(app_lib) - 1);
        app_lib[(al > 0) ? al : 0] = 0;
        host_libs_note(app_lib);
    }

    time_t last_mtime = file_mtime(product);
    int host_pending = 0;   // defer mode: a source change is staged, awaiting the app's go
    pid_t compile_pid = 0;  // async recompile in flight — the app keeps running while it builds
    int apply_compile = 0;  // the in-flight compile was user-requested (defer apply)

    for (;;) {
        if (!do_frame || !do_frame()) break;
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

        // the app asked for another module (a trinity app's orbiter button
        // asks for orbiter): launch it as its own app, with its own window
        // and dock icon, handed this app to hold, and end this one. orbiter
        // opens the app's source with a held instance of it and the app's
        // last picture (SILVER_SWITCH_SHOT, set by the app, rides along)
        typedef const char* (*au_live_take_switch_fn)(void);
        au_live_take_switch_fn take_switch = (au_live_take_switch_fn)dlsym(handle, "au_live_take_switch");
        const char* sw9 = take_switch ? take_switch() : NULL;
#if defined(__linux__)
        // a bound app starts nothing outside its view
        if (sw9 && sw9[0] && g_broker >= 0) {
            fprintf(stderr, "[%s] cannot launch %s from a view\n", name, sw9);
            sw9 = NULL;
        }
#endif
        if (sw9 && sw9[0]) {
            char to_bin[4300];
            snprintf(to_bin, sizeof(to_bin), "%s/%s", bindir, sw9);
            if (access(to_bin, X_OK) != 0 && rebuild_blocking(sw9, 0) != 0) {
                fprintf(stderr, "[%s] cannot launch %s: build failed\n", name, sw9);
            } else {
                char* largv[3] = { to_bin, (char*)name, NULL };
                posix_spawn_file_actions_t lfa;
                posix_spawn_file_actions_init(&lfa);
                const char* lcwd = getenv("SILVER_STARTUP");
                if (lcwd && lcwd[0]) posix_spawn_file_actions_addchdir_np(&lfa, lcwd);
                posix_spawnattr_t lat;
                posix_spawnattr_init(&lat);
                posix_spawnattr_setflags(&lat, POSIX_SPAWN_SETSID);   // outlives this app
                int en = 0;
                while (environ[en]) en++;
                char** lenv = malloc((en + 2) * sizeof(char*));
                int le = 0;
                // a standalone launch: none of this app's host channel
                for (int i = 0; i < en; i++)
                    if (strncmp(environ[i], "SILVER_ISOLATE", 14) != 0 &&
                        strncmp(environ[i], "SILVER_APP_SLOT", 15) != 0 &&
                        strncmp(environ[i], "SILVER_SHM_FD", 13) != 0 &&
                        strncmp(environ[i], "SILVER_DISPLAY", 14) != 0 &&
                        strncmp(environ[i], "SILVER_RELOAD", 13) != 0)
                        lenv[le++] = environ[i];
                lenv[le++] = (char*)"SILVER_START_HELD=1";
                lenv[le] = NULL;
                pid_t lp = 0;
                int lrc = posix_spawn(&lp, to_bin, &lfa, &lat, largv, lenv);
                posix_spawn_file_actions_destroy(&lfa);
                posix_spawnattr_destroy(&lat);
                free(lenv);
                if (lrc != 0) {
                    fprintf(stderr, "[%s] launch %s failed: %s\n", name, sw9, strerror(lrc));
                } else {
                    fprintf(stderr, "[%s] launched %s (pid %d), handing it this app\n", name, sw9, (int)lp);
                    break;   // this app ends: orbiter holds it now
                }
            }
        }

        // watch source files — when any .ag/.c changes.
        // defer must still watch: reload_off only means "never swap the
        // module out from under the app on its own". staging is what
        // raises the reload tab, and skipping the watch entirely left
        // pending stuck at 0 so the tab could never appear.
        int force = 0;
        int changed = 0;
        if (!reload_off || defer)
        for (int i = 0; i < nsr; i++) {
            if (file_mtime(srcs[i].path) != srcs[i].mtime) {
                // the mtime is only consumed when we act on it, so a build
                // already in flight would re-announce this every frame
                if (!compile_pid)
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
                fprintf(stderr, "%s: reload staged — waiting for apply\n", name);
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
            pid_t r;
#if defined(__linux__)
            if (g_broker >= 0)
                r = broker_build_wait(0, &st) ? compile_pid : 0;
            else
#endif
            r = waitpid(compile_pid, &st, WNOHANG);
            if (r == compile_pid) {
                compile_pid = 0;
                if (rebuild_status(st, name) == 0) {
                    // pending stays 2 while the new instance loads: the app
                    // keeps its swap state up to the switch
                    apply_compile = 0;
                    force = 1;   // good build → save + reload below
                } else {
                    // a failed recompile must NOT swap in a broken module or take the app
                    // down — keep the live instance running on the last good build.
                    // a user-requested compile re-arms the reload button to retry.
                    if (apply_compile) {
                        host_pending = 1;
                        if (set_pending) set_pending(1);
                    }
                    apply_compile = 0;
                    fprintf(stderr, "[%s] recompile FAILED — keeping the running build "
                        "(fix the errors above; the app stays up)\n", name);
                }
            } else if (r < 0) {
                compile_pid = 0;
                if (apply_compile) {
                    host_pending = 1;
                    if (set_pending) set_pending(1);
                }
                apply_compile = 0;
            }
        }

        // defer mode: the app asked us to apply the staged change. compile
        // ASYNC — the app keeps rendering, pending=2 shows it compiling.
        // when an external build already left a fresh product, skip straight
        // to the swap.
        // never while a load is in flight: the app's go for the switch is
        // the ready block's to take, not a new apply
        if (defer && host_pending && reload_job.state == 0 && take_apply && take_apply()) {
            host_pending = 0;
            if (!sources_newer(product, srcs, nsr)) {
                if (set_pending) set_pending(0);
                fprintf(stderr, "[%s] apply requested — product fresh, reloading\n", name);
                force = 1;
            } else if (!compile_pid) {
                fprintf(stderr, "[%s] apply requested — recompiling\n", name);
                if (set_pending) set_pending(2);
                apply_compile = 1;
                compile_pid = rebuild_spawn(name, 0);
                if (compile_pid < 0) {
                    compile_pid = 0;
                    apply_compile = 0;
                    host_pending = 1;
                    if (set_pending) set_pending(1);
                }
            }
        }

        // watch product symlink — reload when .so is relinked (external builds).
        // while our own compile is in flight the product relinks BEFORE silver
        // finishes — the reap above owns that reload, so stand down until then.
        // force bypasses reload_off: it only arises from a good build the user
        // asked for (defer apply) — reload_off gates only self-initiated swaps
        time_t cur = file_mtime(product);
        // defer mode: an outside build (the language service, an agent)
        // only stages the swap; the app's apply takes it, fresh as it is
        if (defer && !force && !compile_pid && cur && cur != last_mtime
                && reload_job.state == 0) {
            last_mtime = cur;
            if (!host_pending) {
                host_pending = 1;
                if (set_pending) set_pending(1);
                fprintf(stderr, "%s: product rebuilt — reload staged, waiting for apply\n", name);
            }
        }
        // mtime 0: the linker has the product unlinked, not a build
        if (force || (!reload_off && !defer && !compile_pid && cur && cur != last_mtime
                      && reload_job.state == 0)) {   // one load at a time
            last_mtime = cur;
            // a library under it rebuilt since launch: no swap, a restart
            const char* moved = host_libs_changed();
            if (moved) {
                fprintf(stderr, "[%s] reload refused: %s was rebuilt since "
                        "launch; restart to load it\n", name, moved);
                if (set_pending) set_pending(1);
                continue;
            }
            char cwd_now[4096];
            if (!getcwd(cwd_now, sizeof(cwd_now))) cwd_now[0] = 0;
            fprintf(stderr, "[%s] reloading (cwd %s)\n", name, cwd_now);

            // the new image loads and inits BESIDE the live instance, which
            // keeps framing: its init runs on a worker inside an Au space
            // (its types register there, not over the live ones) and only
            // the switch below is a frame's gap
            n = readlink(product, lib, sizeof(lib) - 1);
            if (n < 0) break;
            lib[n] = '\0';
            void* new_handle = reload_dlopen(lib, cur);
            // an external writer (an agent rebuilding sources) can relink the
            // .so while we copy it — the torn copy fails to load. wait for the
            // write to finish and recopy instead of dying.
            for (int rt = 0; (!new_handle || new_handle == handle) && rt < 10; rt++) {
                fprintf(stderr, "%s: reload copy torn — retrying (%d)\n", name, rt + 1);
                usleep(300000);
                n = readlink(product, lib, sizeof(lib) - 1);
                if (n < 0) break;
                lib[n] = '\0';
                new_handle = reload_dlopen(lib, file_mtime(product));
            }
            if (!new_handle || new_handle == handle) {
                fprintf(stderr, "[%s] reload failed: %s\n", name, dlerror());
                if (set_pending) set_pending(1);
                continue;   // the live instance goes on
            }
            // persist slots: held from the live instance, in the new image's
            // slots before its init; both see them until the switch
            au_persist_fn psave = (au_persist_fn)dlsym(RTLD_DEFAULT, "au_persist_save");
            au_persist_fn pload = (au_persist_fn)dlsym(RTLD_DEFAULT, "au_persist_load");
            int npersist = psave ? psave(handle) : 0;
            if (npersist && pload) {
                pload(new_handle);
                fprintf(stderr, "[%s] %d persist slot(s) kept\n", name, npersist);
            }
            // SILVER_RELOAD_LOAD stays set afterward: every subsequent init in
            // this process IS a reload, so the fresh instance may restore the
            // flash state. PARALLEL tells its run to leave the swapchain alone.
            setenv("SILVER_RELOAD_LOAD", "1", 1);
            setenv("SILVER_RELOAD_PARALLEL", "1", 1);
            reload_job.handle = new_handle;
            reload_job.space  = NULL;
            reload_job.argc   = argc;
            reload_job.argv   = argv;
            reload_job.state  = 1;
            if (pthread_create(&reload_job.thread, NULL, reload_worker, &reload_job) != 0) {
                fprintf(stderr, "[%s] reload: no worker thread\n", name);
                unsetenv("SILVER_RELOAD_PARALLEL");
                au_persist_fn prelease = (au_persist_fn)dlsym(RTLD_DEFAULT, "au_persist_release");
                if (prelease) prelease(new_handle);
                dlclose(new_handle);
                reload_job.state = 0;
            }
        }

        // the switch: the new instance is built, so at this frame boundary
        // the live one hands over the frame. its teardown runs on a thread:
        // the main thread only stops its watches and takes it out of the
        // registry, so the new instance's next frame is the whole gap
        // the new instance is ready: pending 3 tells the app, which fades
        // what it has to fade and hands back the go; the switch waits for
        // that go, or 800 ms for an app with nothing to fade
        if (reload_job.state == 2 && !reload_job.ready_at) {
            reload_job.ready_at = now_ms();
            if (set_pending) set_pending(3);
            au_live_take_apply_fn take_go = (au_live_take_apply_fn)dlsym(RTLD_DEFAULT, "au_live_take_apply");
            if (take_go) take_go();   // clear a stale request
        }
        if (reload_job.state == 2 && reload_job.ready_at) {
            au_live_take_apply_fn take_go = (au_live_take_apply_fn)dlsym(RTLD_DEFAULT, "au_live_take_apply");
            int go = take_go ? take_go() : 1;
            if (!go && now_ms() - reload_job.ready_at < 800) continue;
        }
        if (reload_job.state == 2) {
            reload_job.ready_at = 0;
            pthread_join(reload_job.thread, NULL);
            void* new_handle = reload_job.handle;
            long t0 = now_ms();
            // the live instance's last frame is done: a take it holds goes out
            void (*handoff)(void) = (void (*)(void))dlsym(RTLD_DEFAULT, "au_live_handoff");
            if (handoff) handoff();
            // a watch still running in the old image calls freed code on its
            // next event: stop those before the registry purge and the close
            watch_pause_image_fn wpause = (watch_pause_image_fn)dlsym(handle, "watch_pause_image");
            if (wpause) wpause((void*)do_init);
            // the old module leaves the registry before the new one enters it:
            // a name lookup must never land on the old types again
            module_erase_fn merase = (module_erase_fn)dlsym(RTLD_DEFAULT, "module_erase");
            find_module_fn  fmod   = (find_module_fn)dlsym(RTLD_DEFAULT, "find_module");
#ifdef SILVER_SHARE_NAME
            const char* reg_name = SILVER_SHARE_NAME;
#else
            const char* reg_name = name;
#endif
            if (merase && fmod) { void* old_mod = fmod(reg_name); if (old_mod) merase(old_mod, NULL); }
            module_purge_image_fn purge = (module_purge_image_fn)dlsym(handle, "module_purge_image");
            if (purge) purge((void*)do_init);
            au_space_promote_fn promote = (au_space_promote_fn)dlsym(RTLD_DEFAULT, "au_space_promote");
            if (promote) promote(reload_job.space);
            unsetenv("SILVER_RELOAD_PARALLEL");
            au_persist_fn prelease = (au_persist_fn)dlsym(RTLD_DEFAULT, "au_persist_release");
            if (prelease) prelease(handle);

            if (close_job.state == 1) close_job_finish();
            close_job.handle  = handle;
            close_job.destroy = do_destroy;
            close_job.image   = (void*)do_init;
            close_job.name    = name;
            handle    = new_handle;
            do_init   = dlsym(handle, INIT_SYM);
            do_frame  = dlsym(handle, FRAME_SYM);
            do_destroy= dlsym(handle, DESTROY_SYM);
            reload_job.state = 0;
            if (set_pending) set_pending(0);
            fprintf(stderr, "[%s] switch: %ldms\n", name, now_ms() - t0);
            // SILVER_RELOAD_SAVE is set for the reload-path destroy only (the
            // final exit destroy never sees it) — apps use it to flash-save
            setenv("SILVER_RELOAD_SAVE", "1", 1);
            close_job.state = 1;
            if (pthread_create(&close_job.thread, NULL, close_worker, &close_job) != 0) {
                // on this thread its own drain is the main pool's
                close_job.drained = 1;
                close_worker(&close_job);
                close_job.state = close_job.done = close_job.destroyed = close_job.drained = 0;
            }
            fprintf(stderr, "[%s] reload complete\n", name);

            // refresh source watch list from new artifacts
            load_sources(artifacts, srcs, &nsr);
        }
        // the old instance is destroyed: drain this thread's pool while its
        // image is still mapped, then the close thread may unmap it
        if (close_job.state == 1 && __atomic_load_n(&close_job.destroyed, __ATOMIC_ACQUIRE)
            && !close_job.drained) {
            au_auto_free_fn afree = (au_auto_free_fn)dlsym(RTLD_DEFAULT, "auto_free");
            if (afree) afree();
            __atomic_store_n(&close_job.drained, 1, __ATOMIC_RELEASE);
        }
        // the old instance is gone: the flash-save flag goes with it
        if (close_job.state == 1 && __atomic_load_n(&close_job.done, __ATOMIC_ACQUIRE)) {
            close_job_finish();
            unsetenv("SILVER_RELOAD_SAVE");
        }
    }

    // sound stops before the app's memory is freed: its audio
    // threads would play the freed buffers as noise
    void (*quiet)(void) = handle ? dlsym(handle, "spectra_quiet") : NULL;
    if (quiet) quiet();
    if (do_destroy) do_destroy();
    if (handle)     dlclose(handle);
    return 0;
}

#ifdef _WIN32
// linked /SUBSYSTEM:WINDOWS for GUI apps, which enter here rather than at
// main(). a console subsystem binary opens a console beside every launch, and
// build output already goes to the app's log file
int __stdcall WinMain(void* instance, void* prev, char* cmdline, int show) {
    (void)instance; (void)prev; (void)cmdline; (void)show;
    return main(__argc, __argv);
}
#endif
