// ios app host: uikit owns the loop, so the product's frame runs from
// platform_run's tick. no live reload here: the bundle is sealed by its
// signature, and the product loads from Frameworks/ beside this binary
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <unistd.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#include "host.h"

typedef void (*init_fn)(void);
typedef int  (*frame_fn)(void);
typedef void (*au_main_args_fn)(int, char**);

static void*    g_handle;
static init_fn  g_init;
static frame_fn g_frame;
static int      g_argc;
static char**   g_argv;

static char g_dir[4096];

static bool tick(void* ctx) {
    if (!g_handle) {
        // a bare name is not searched by rpath on ios: name the file
        char lib[4096];
        snprintf(lib, sizeof(lib), "%s/Frameworks/%s", g_dir, SILVER_PRODUCT);
        g_handle = dlopen(lib, RTLD_NOW);
        if (!g_handle) { fprintf(stderr, "silver-host: dlopen %s: %s\n", lib, dlerror()); return false; }
        au_main_args_fn set_args = (au_main_args_fn)dlsym(RTLD_DEFAULT, "au_main_args");
        if (set_args) set_args(g_argc, g_argv);
        g_init  = (init_fn) dlsym(g_handle, "silver_live_init");
        g_frame = (frame_fn)dlsym(g_handle, "silver_live_frame");
        if (g_init) g_init();
        return g_frame != NULL;
    }
    return g_frame() != 0;
}

int main(int argc, char** argv) {
    g_argc = argc; g_argv = argv;
    // resources sit beside the binary: <App>.app/share/<name>
    char exe[4096]; uint32_t n = sizeof(exe);
    if (_NSGetExecutablePath(exe, &n) == 0) {
        snprintf(g_dir, sizeof(g_dir), "%s", dirname(exe));
        char share[4096];
        snprintf(share, sizeof(share), "%s/share/%s", g_dir, SILVER_SHARE_NAME);
        chdir(share);
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    platform_init();
    return platform_run(tick, NULL);
}
