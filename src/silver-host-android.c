// android app host: NativeActivity loads this library by name and devices
// runs main on its own thread. no live reload: the package is sealed by its
// signature, and the product loads by soname from the package's lib dir.
// share/<name> ships as assets and is laid out in the app's own files dir
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include "host.h"

typedef void (*init_fn)(void);
typedef int  (*frame_fn)(void);
typedef void (*au_main_args_fn)(int, char**);

static ANativeActivity* g_activity;
static void*            g_handle;
static init_fn          g_init;
static frame_fn         g_frame;

static bool tick(void* ctx) {
    if (!g_handle) {
        g_handle = dlopen(SILVER_PRODUCT, RTLD_NOW);
        if (!g_handle) { fprintf(stderr, "silver-host: dlopen %s: %s\n", SILVER_PRODUCT, dlerror()); return false; }
        au_main_args_fn set_args = (au_main_args_fn)dlsym(RTLD_DEFAULT, "au_main_args");
        static char* argv[] = { (char*)SILVER_SHARE_NAME, NULL };
        if (set_args) set_args(1, argv);
        g_init  = (init_fn) dlsym(g_handle, "silver_live_init");
        g_frame = (frame_fn)dlsym(g_handle, "silver_live_frame");
        if (g_init) g_init();
        return g_frame != NULL;
    }
    return g_frame() != 0;
}

// stdout and stderr have nowhere to go on a phone: a pipe carries every
// line to logcat under the silver tag, where `silver -d android` reads it
static void* log_pump(void* arg) {
    int   fd = (int)(long)arg;
    char  buf[4096];
    int   n  = 0;
    for (;;) {
        char c;
        if (read(fd, &c, 1) <= 0) break;
        if (c == '\n' || n == sizeof(buf) - 1) {
            buf[n] = 0;
            __android_log_write(ANDROID_LOG_INFO, "silver", buf);
            n = 0;
        } else buf[n++] = c;
    }
    return NULL;
}

static void mkdirs(const char* p) {
    char t[1024];
    snprintf(t, sizeof(t), "%s", p);
    for (char* s = t + 1; *s; s++)
        if (*s == '/') { *s = 0; mkdir(t, 0755); *s = '/'; }
}

static char* asset_read(AAssetManager* am, const char* name, size_t* len) {
    AAsset* as = AAssetManager_open(am, name, AASSET_MODE_BUFFER);
    if (!as) return NULL;
    size_t n = AAsset_getLength(as);
    char*  d = malloc(n + 1);
    AAsset_read(as, d, n);
    AAsset_close(as);
    d[n] = 0;
    if (len) *len = n;
    return d;
}

// assets/share.list names every file, since the asset dir lists no
// subdirectories; the stamp says whether this package was extracted before
static void extract_share(const char* dir) {
    AAssetManager* am = g_activity->assetManager;
    char*  stamp = asset_read(am, "share.stamp", NULL);
    char   stamp_file[1024];
    snprintf(stamp_file, sizeof(stamp_file), "%s/share.stamp", dir);
    FILE*  sf = fopen(stamp_file, "rb");
    if (sf && stamp) {
        char have[256] = {0};
        fread(have, 1, sizeof(have) - 1, sf);
        fclose(sf);
        if (strcmp(have, stamp) == 0) { free(stamp); return; }
    } else if (sf) fclose(sf);
    char* list = asset_read(am, "share.list", NULL);
    for (char* ln = list; ln && *ln; ) {
        char* end = strchr(ln, '\n');
        if (end) *end = 0;
        char   src[1024], dst[1024];
        snprintf(src, sizeof(src), "share/%s/%s", SILVER_SHARE_NAME, ln);
        snprintf(dst, sizeof(dst), "%s/share/%s/%s", dir, SILVER_SHARE_NAME, ln);
        size_t n;
        char*  d = asset_read(am, src, &n);
        if (d) {
            mkdirs(dst);
            FILE* f = fopen(dst, "wb");
            if (f) { fwrite(d, 1, n, f); fclose(f); }
            free(d);
        }
        ln = end ? end + 1 : NULL;
    }
    free(list);
    if (stamp) {
        mkdirs(stamp_file);
        FILE* f = fopen(stamp_file, "wb");
        if (f) { fputs(stamp, f); fclose(f); }
        free(stamp);
    }
}

static int host_main(int argc, char** argv) {
    int fds[2];
    if (pipe(fds) == 0) {
        dup2(fds[1], 1);
        dup2(fds[1], 2);
        pthread_t t;
        pthread_create(&t, NULL, log_pump, (void*)(long)fds[0]);
        pthread_detach(t);
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    const char* dir = g_activity->internalDataPath;
    // bionic sets no HOME or /tmp; the app's private data dir is the only
    // writable root, so point the path helpers (cache/storage/config) and
    // tmpfile there — they resolve through HOME/TMPDIR like any posix app
    setenv("HOME", dir, 1);
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s/cache", dir);
    mkdirs(tmp); mkdir(tmp, 0755);
    setenv("TMPDIR", tmp, 1);
    extract_share(dir);
    char share[1024];
    snprintf(share, sizeof(share), "%s/share/%s", dir, SILVER_SHARE_NAME);
    mkdirs(share);
    mkdir(share, 0755);
    chdir(share);
    platform_init();
    return platform_run(tick, NULL);
}

__attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity* activity, void* saved, size_t saved_size) {
    g_activity = activity;
    platform_android_start(activity, host_main);
}
