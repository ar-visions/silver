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

typedef int        (*frame_fn)(void);
typedef void       (*destroy_fn)(void);
typedef void       (*init_fn)(void);
typedef int        (*au_compile_ready_fn)(void);
typedef void       (*au_compile_invoke_fn)(const char*);
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
#if defined(__SANITIZE_ADDRESS__) || (defined(__has_feature) && __has_feature(address_sanitizer))
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
static void rebuild_blocking(const char* name) {
    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
        "cd \"" SILVER_ROOT "\" && \"" SILVER_ROOT "/platform/native/debug/silver\" %s",
        name);
    fprintf(stdout, "%s: source changed — rebuilding\n", name);
    fflush(stdout);
    system(cmd);
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

    char self[4096];
    strncpy(self, argv[0], sizeof(self) - 1);
    const char* name   = basename(self);
    strncpy(self, argv[0], sizeof(self) - 1);
    const char* bindir = dirname(self);
    g_app_name = name;

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
        rebuild_blocking(name);
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
    if (do_init) do_init();

    time_t last_mtime = file_mtime(product);

    while (do_frame && do_frame()) {
        // watch source files — trigger recompile when any .ag or .c changes
        int force = 0;
        for (int i = 0; i < nsr; i++) {
            time_t cur = file_mtime(srcs[i].path);
            if (cur != srcs[i].mtime) {
                fprintf(stdout, "%s: source changed: %s\n", name, srcs[i].path);
                // use handle (not RTLD_DEFAULT) so we find symbols in transitive deps (libAu.so)
                au_compile_ready_fn  ready_fn  = (au_compile_ready_fn)dlsym(handle,  "au_compile_ready");
                au_compile_invoke_fn invoke_fn = (au_compile_invoke_fn)dlsym(handle, "au_compile_invoke");
                fprintf(stdout, "%s: ready_fn=%p invoke_fn=%p ready=%d\n", name,
                    (void*)ready_fn, (void*)invoke_fn, ready_fn ? ready_fn() : -1);
                if (ready_fn && ready_fn() && invoke_fn) {
                    invoke_fn(name);
                    force = 1;
                } else {
                    char cmd[8192];
                    snprintf(cmd, sizeof(cmd),
                        SILVER_ROOT "/platform/native/debug/silver %s &", name);
                    system(cmd);
                }
                // refresh all source mtimes so we don't retrigger until next edit
                for (int j = 0; j < nsr; j++)
                    srcs[j].mtime = file_mtime(srcs[j].path);
                break;
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
