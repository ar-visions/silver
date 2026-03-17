#include <silver/import>

static void ensure_platform(silver a) {
    if (!a->platform) return;
    path platform_path = f(path, "%o/platform/%o", a->install, a->platform);
    path clang_bin     = f(path, "%o/platform/native/bin", a->install);
    if (dir_exists("%o", platform_path)) return;

    // android targets: fetch NDK and extract sysroot
    if (cmp(a->platform, "android-arm64") == 0 || cmp(a->platform, "android-x86_64") == 0) {
        cstr ndk_ver    = "r27";
        cstr api_level  = "33";
        bool is_arm64   = cmp(a->platform, "android-arm64") == 0;
        cstr arch       = is_arm64 ? "aarch64" : "x86_64";
        cstr processor  = is_arm64 ? "aarch64" : "x86_64";
        char triple[64];
        snprintf(triple, sizeof(triple), "%s-linux-android%s", arch, api_level);

        // determine host platform for NDK download
#ifdef __APPLE__
        cstr host = "darwin";
#elif defined(__linux__)
        cstr host = "linux";
#else
        verify(false, "unsupported host platform for android platform fetch");
#endif

        path ndk_zip  = f(path, "%o/platform/android-ndk-%s-%s.zip", a->install, ndk_ver, host);
        path ndk_dir  = f(path, "%o/platform/android-ndk-%s",        a->install, ndk_ver);

        // download NDK if not cached
        if (!file_exists("%o", ndk_zip)) {
            vexec(true, "ndk-fetch",
                "curl -L -o %o "
                "https://dl.google.com/android/repository/android-ndk-%s-%s.zip",
                ndk_zip, ndk_ver, host);
            verify(file_exists("%o", ndk_zip), "NDK download failed");
        }

        // extract if not already
        if (!dir_exists("%o", ndk_dir)) {
            vexec(true, "ndk-extract", "unzip -q %o -d %o/platform", ndk_zip, a->install);
            verify(dir_exists("%o", ndk_dir), "NDK extraction failed");
        }

        // create platform target dir with symlink to sysroot
        vexec(true, "platform-mkdir", "mkdir -p %o", platform_path);
        char sysroot[512];
        snprintf(sysroot, sizeof(sysroot),
            "%s/toolchains/llvm/prebuilt/%s-x86_64/sysroot",
            ndk_dir->chars, host);
        verify(dir_exists("%s", sysroot), "NDK sysroot not found at %s", sysroot);

        // symlink sysroot into our platform dir
        vexec(true, "platform-sysroot", "ln -s %s %o/sysroot", sysroot, platform_path);

        // generate target.cmake
        write_target_cmake(platform_path, "Android", processor, triple, sysroot, clang_bin);
        return;
    }

    verify(false, "unknown platform target: %o", a->platform);
}
