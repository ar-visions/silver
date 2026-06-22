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

extern char** environ;

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

static void crash_handler(int sig) {
    void*  frames[64];
    int    nframes = backtrace(frames, 64);
    char** syms    = backtrace_symbols(frames, nframes);
    fprintf(stderr, "\n%s: signal %d (%s)\n", g_app_name, sig,
        sig == SIGSEGV ? "SIGSEGV" :
        sig == SIGABRT ? "SIGABRT" :
        sig == SIGBUS  ? "SIGBUS"  :
        sig == SIGILL  ? "SIGILL"  :
        sig == SIGFPE  ? "SIGFPE"  : "?");
    for (int i = 0; i < nframes; i++)
        fprintf(stderr, "  #%d %s\n", i, syms ? syms[i] : "??");
    free(syms);
    signal(sig, SIG_DFL);
    raise(sig);
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
static int rebuild_blocking(const char* name) {
    char cmd[8192];
    // --build: compile ONLY. bare `silver <app>` would LAUNCH the app (silver_live_run execs
    // the live host), spawning a whole second process+window on every reload while this one
    // keeps running. we just want the fresh .so produced so the host below hot-swaps it.
    snprintf(cmd, sizeof(cmd),
        "cd \"" SILVER_ROOT "\" && \"" SILVER_ROOT "/platform/native/debug/silver\" %s --build",
        name);
    fprintf(stdout, "%s: source changed — rebuilding\n", name);
    fflush(stdout);
    // posix_spawn, NOT system(): system() fork()s, and this host is a multithreaded
    // GUI process (MoltenVK / libdispatch / GLFW). fork() from a multithreaded process
    // copies only the calling thread but inherits locks held by the others — the child
    // can deadlock on a malloc lock before it reaches exec, freezing the whole app.
    // posix_spawn execs without running user code in the child, so it can't deadlock.
    char* sh_argv[] = { "/bin/sh", "-c", cmd, NULL };
    pid_t pid = 0;
    int sp = posix_spawn(&pid, "/bin/sh", NULL, NULL, sh_argv, environ);
    if (sp != 0) {
        fprintf(stderr, "%s: BUILD ERROR — posix_spawn failed: %s\n", name, strerror(sp));
        return -1;
    }
    int rc = 0;
    if (waitpid(pid, &rc, 0) < 0) {
        fprintf(stderr, "%s: BUILD ERROR — waitpid failed\n", name);
        return -1;
    }
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

// recompile via the in-process compiler if the app published one, else shell out to silver.
static int do_recompile(void* handle, const char* name) {
    au_compile_ready_fn  ready_fn  = (au_compile_ready_fn) dlsym(handle,  "au_compile_ready");
    au_compile_invoke_fn invoke_fn = (au_compile_invoke_fn)dlsym(handle, "au_compile_invoke");
    if (ready_fn && ready_fn() && invoke_fn) {
        invoke_fn(name);   // in-process compiler reports its own errors
        return 0;
    }
    return rebuild_blocking(name);
}

int main(int argc, char** argv) {
    printf("silver-host main\n");

#ifdef SILVER_ROOT
    setenv("LD_LIBRARY_PATH",
        SILVER_ROOT "/platform/native/lib:"
        SILVER_ROOT "/install/lib:"
        SILVER_ROOT "/install/debug",
        1);
#endif

    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGBUS,  crash_handler);
    signal(SIGILL,  crash_handler);
    signal(SIGFPE,  crash_handler);

    // resolve argv[0] to an absolute path (also follows a PATH symlink), so the
    // product/source paths below survive the cd_share() that changes cwd —
    // otherwise a relative bindir breaks file_mtime()/readlink() after the cd.
    char abspath[4096];
    if (!realpath(argv[0], abspath)) {
        strncpy(abspath, argv[0], sizeof(abspath) - 1);
        abspath[sizeof(abspath) - 1] = '\0';
    }
    char self[4096], self2[4096];
    strncpy(self,  abspath, sizeof(self)  - 1); self[sizeof(self)   - 1] = '\0';
    strncpy(self2, abspath, sizeof(self2) - 1); self2[sizeof(self2) - 1] = '\0';
    const char* name   = basename(self);
    const char* bindir = dirname(self2);
    g_app_name = name;

    // --defer-reload (or SILVER_DEFER_RELOAD): don't auto-recompile on a source change —
    // stage it, signal the app it's pending, and only recompile + hot-swap when the app
    // requests it (au_live_request_apply). lets orbiter show a "reload ready" affordance.
    // deferred live-reload is normally turned ON BY THE APP at runtime (it calls
    // au_live_set_defer; the watch loop below polls au_live_get_defer each iteration). the
    // user passes nothing. SILVER_DEFER_RELOAD is just a dev override that forces it on.
    int host_defer = (getenv("SILVER_DEFER_RELOAD") != NULL);

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
    cd_share(bindir, name);

    // up-front staleness check: only recompile when a source is actually newer
    // than the built product. when nothing changed we skip the compile entirely
    // and dlopen the existing .so — no rebuild on every launch.
    source_watch srcs[MAX_SOURCES];
    int nsr = 0;
    load_sources(artifacts, srcs, &nsr);
    if (sources_newer(product, srcs, nsr)) {
        if (rebuild_blocking(name) != 0) {
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
    if (do_init) do_init();

    time_t last_mtime = file_mtime(product);
    int host_pending = 0;   // defer mode: a source change is staged, awaiting the app's go

    while (do_frame && do_frame()) {
        // app<->host signals (resolved each iter — handle changes across reloads)
        au_live_set_pending_fn set_pending = (au_live_set_pending_fn)dlsym(handle, "au_live_set_pending");
        au_live_take_apply_fn  take_apply  = (au_live_take_apply_fn) dlsym(handle, "au_live_take_apply");
        au_live_get_defer_fn   get_defer   = (au_live_get_defer_fn)  dlsym(handle, "au_live_get_defer");
        // defer is dynamic: the app turns it on (au_live_set_defer); env forces it for devs.
        int defer = host_defer || (get_defer && get_defer());

        // watch source files — when any .ag/.c changes
        int force = 0;
        int changed = 0;
        for (int i = 0; i < nsr; i++) {
            if (file_mtime(srcs[i].path) != srcs[i].mtime) {
                fprintf(stdout, "%s: source changed: %s\n", name, srcs[i].path);
                changed = 1;
            }
        }
        if (changed) {
            // refresh all source mtimes so we don't retrigger until the next edit
            for (int j = 0; j < nsr; j++)
                srcs[j].mtime = file_mtime(srcs[j].path);
            if (defer) {
                // stage it: signal the app a reload is pending; recompile only on its request
                host_pending = 1;
                if (set_pending) set_pending(1);
            } else {
                // recompile SYNCHRONOUSLY so we can see whether it succeeded
                int rc = do_recompile(handle, name);
                if (rc == 0) {
                    force = 1;   // good build → reload below
                } else {
                    // a failed recompile must NOT swap in a broken module or take the app
                    // down — keep the live instance running on the last good build.
                    fprintf(stderr, "%s: recompile FAILED — keeping the running build "
                        "(fix the errors above; the app stays up)\n", name);
                }
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

        // watch product symlink — reload when .so is relinked
        time_t cur = file_mtime(product);
        if (force || cur != last_mtime) {
            last_mtime = cur;
            fprintf(stderr, "%s: reloading\n", name);

            // Load new .so FIRST so dependency refcounts are bumped before old is closed.
            n = readlink(product, lib, sizeof(lib) - 1);
            if (n < 0) break;
            lib[n] = '\0';
            void* new_handle = reload_dlopen(lib, cur);
            if (!new_handle) {
                fprintf(stderr, "%s: reload failed: %s\n", name, dlerror());
                return 1;
            }

            // Destroy old instance (uses old code/inst_g) then release old handle
            if (do_destroy) do_destroy();
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

    if (do_destroy) do_destroy();
    if (handle)     dlclose(handle);
    return 0;
}
