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

typedef int  (*frame_fn)(void);
typedef void (*destroy_fn)(void);
#define FRAME_SYM   "silver_live_frame"
#define DESTROY_SYM "silver_live_destroy"

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
    for (int i = 0; i < 20 && !h; i++) {
        h = dlopen(lib, RTLD_NOW | RTLD_GLOBAL);
        if (!h) usleep(50000);  // 50ms retry — handles linker write race
    }
    return h;
}

int main(int argc, char** argv) {
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

    char lib[4096];
    ssize_t n = readlink(product, lib, sizeof(lib) - 1);
    if (n < 0) {
        fprintf(stderr, "%s: no product file at %s\n", name, product);
        return 1;
    }
    lib[n] = '\0';

    cd_share(bindir, name);

    void* handle = try_dlopen(lib);
    if (!handle) { fprintf(stderr, "%s: dlopen %s: %s\n", name, lib, dlerror()); return 1; }

    frame_fn   do_frame   = dlsym(handle, FRAME_SYM);
    destroy_fn do_destroy = dlsym(handle, DESTROY_SYM);

    int    live       = getenv("SILVER") != NULL;
    time_t last_mtime = live ? file_mtime(product) : 0;

    while (do_frame && do_frame()) {
        if (!live) continue;
        time_t cur = file_mtime(product);
        if (cur != last_mtime) {
            last_mtime = cur;
            fprintf(stderr, "%s: reloading\n", name);
            if (do_destroy) do_destroy();
            dlclose(handle);
            n = readlink(product, lib, sizeof(lib) - 1);
            if (n < 0) break;
            lib[n] = '\0';
            handle = try_dlopen(lib);
            if (!handle) {
                fprintf(stderr, "%s: reload failed: %s\n", name, dlerror());
                return 1;
            }
            do_frame   = dlsym(handle, FRAME_SYM);
            do_destroy = dlsym(handle, DESTROY_SYM);
            fprintf(stderr, "%s: reload complete\n", name);
        }
    }

    if (do_destroy) do_destroy();
    if (handle)     dlclose(handle);
    return 0;
}
