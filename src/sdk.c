#include <silver/import>

static void ensure_sdk(silver a) {
    if (!a->sdk) return;
    path sdk_path   = f(path, "%o/sdk/%o", a->install, a->sdk);
    path clang_bin  = f(path, "%o/sdk/native/bin", a->install);
    if (dir_exists("%o", sdk_path)) return;

    // android targets: fetch NDK and extract sysroot
    if (cmp(a->sdk, "android-arm64") == 0 || cmp(a->sdk, "android-x86_64") == 0) {
        cstr ndk_ver    = "r27";
        cstr api_level  = "33";
        bool is_arm64   = cmp(a->sdk, "android-arm64") == 0;
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
        verify(false, "unsupported host platform for android SDK fetch");
#endif

        path ndk_zip  = f(path, "%o/sdk/android-ndk-%s-%s.zip", a->install, ndk_ver, host);
        path ndk_dir  = f(path, "%o/sdk/android-ndk-%s",        a->install, ndk_ver);

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
            vexec(true, "ndk-extract", "unzip -q %o -d %o/sdk", ndk_zip, a->install);
            verify(dir_exists("%o", ndk_dir), "NDK extraction failed");
        }

        // create sdk target dir with symlink to sysroot
        vexec(true, "sdk-mkdir", "mkdir -p %o", sdk_path);
        char sysroot[512];
        snprintf(sysroot, sizeof(sysroot),
            "%s/toolchains/llvm/prebuilt/%s-x86_64/sysroot",
            ndk_dir->chars, host);
        verify(dir_exists("%s", sysroot), "NDK sysroot not found at %s", sysroot);

        // symlink sysroot into our sdk dir
        vexec(true, "sdk-sysroot", "ln -s %s %o/sysroot", sysroot, sdk_path);

        // generate target.cmake
        write_target_cmake(sdk_path, "Android", processor, triple, sysroot, clang_bin);
        return;
    }

    verify(false, "unknown sdk target: %o", a->sdk);
}