// silver2 ios: the .app bundle, its Frameworks closure, the profile and
// codesign, simctl and devicectl. Included by silver2.c: it shares that
// translation unit.

static void
ios_bundle_dylibs(const char* bin, const char* frameworks,
                  List* done) { // otool's closure, each carried in
                                // Frameworks/ and named @rpath
    char* root =
        format("%s/platform/%s", S->silver_root, S->target_dir);
    FILE* pipe = popen(format("otool -L %s", bin), "r");
    char  line[1024];
    bool  first = true;
    while (pipe && fgets(line, sizeof line, pipe)) {
        if (first) {
            first = false;
            continue;
        }
        char* t = line;
        while (*t == ' ' || *t == '\t') t++;
        char* paren = strstr(t, " (");
        if (!paren) continue;
        *paren           = 0;
        const char* src  = 0;
        char*       leaf = 0;
        if (!strncmp(t, "@rpath/", 7)) {
            leaf           = strdup(t + 7);
            const char *c1 = format("%s/install/lib/%s", S->out_dir,
                                    leaf),
                       *c2 = format("%s/lib/%s", root, leaf);
            src = !access(c1, R_OK) ? c1 : !access(c2, R_OK) ? c2 : 0;
        } else if (t[0] == '/' && !strncmp(t, S->silver_root,
                                           strlen(S->silver_root))) {
            src  = strdup(t);
            leaf = strdup(strrchr(t, '/') + 1);
            run_shell(
                format("install_name_tool -change %s @rpath/%s %s", t,
                       leaf, bin));
        }
        if (!src || !leaf) continue;
        bool seen = false;
        for (int i = 0; i < done->count; i++)
            if (same(done->data[i], leaf)) seen = true;
        if (seen) continue;
        list_push(done, leaf);
        char* dst = format("%s/%s", frameworks, leaf);
        run_shell(format("cp -L %s %s && chmod u+w %s && "
                         "install_name_tool -id @rpath/%s %s",
                         src, dst, dst, leaf, dst));
        ios_bundle_dylibs(dst, frameworks, done);
    }
    if (pipe) pclose(pipe);
}
static char* ios_bundle(
    const char* product, const char* install_dir,
    const char* module_dir) { // <out>/<name>.app: the ios host,
                              // Frameworks/ with the product and its
                              // closure, share, the profile, signatures
    const char* name = S->modname;
    char*       root =
        format("%s/platform/%s", S->silver_root, S->target_dir);
    char *app           = format("%s/%s.app", S->out_dir, name),
         *frameworks    = format("%s/Frameworks", app);
    bool        sim     = strstr(S->platform, "simulator") != 0;
    const char* version = bundle_version(module_dir);
    run_shell(format("rm -rf %s && mkdir -p %s", app, frameworks));
    fprintf(stderr, "[%s] ios: staging %s\n", name, app);
    char*       exe  = format("%s/%s", app, name);
    const char* leaf = strrchr(product, '/') + 1;
    if (run_shell(format(
            "%s/clang -target %s -isysroot %s -fuse-ld=lld -B%s %s "
            "-I%s/devices -DSILVER_PRODUCT='\"%s\"' "
            "-DSILVER_SHARE_NAME='\"%s\"' %s/src/silver-host-ios.c %s "
            "-L%s/lib -lAu -framework UIKit -framework Foundation "
            "-Wl,-rpath,@executable_path/Frameworks -o %s",
            S->tools, S->triple, S->sysroot, S->tools,
            S->release ? "-O2" : "-g", S->silver_root, leaf, name,
            S->silver_root, devices_lib("dylib"), root, exe))) {
        fprintf(stderr, "ios: host link failed\n");
        exit(1);
    }
    List done = {0};
    run_shell(format("cp -L %s %s/%s && chmod u+w %s/%s && "
                     "install_name_tool -id @rpath/%s %s/%s",
                     product, frameworks, leaf, frameworks, leaf, leaf,
                     frameworks, leaf));
    list_push(&done, strdup(leaf));
    ios_bundle_dylibs(format("%s/%s", frameworks, leaf), frameworks,
                      &done);
    ios_bundle_dylibs(exe, frameworks, &done);
    char* share = format("%s/share/%s", install_dir, name);
    if (!access(share, R_OK))
        run_shell(format("mkdir -p %s/share && cp -RL %s %s/share/%s",
                         app, share, app, name));
    bool landscape = !run_shell(format(
        "grep -q '^export landscape: *true' %s/%s.ag", module_dir,
        name)); // `export landscape: true` in the module
    save_text(
        format("%s/Info.plist", app),
        format(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE "
            "plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
            "\"http://www.apple.com/DTDs/"
            "PropertyList-1.0.dtd\">\n<plist version=\"1.0\"><dict>\n"
            "  <key>CFBundleName</key><string>%s</string>\n  "
            "<key>CFBundleDisplayName</key><string>%s</string>\n  "
            "<key>CFBundleIdentifier</key><string>com.silver.%s</"
            "string>\n  "
            "<key>CFBundleExecutable</key><string>%s</string>\n"
            "  <key>CFBundlePackageType</key><string>APPL</string>\n  "
            "<key>CFBundleVersion</key><string>%s</string>\n  "
            "<key>CFBundleShortVersionString</key><string>%s</"
            "string>\n  "
            "<key>CFBundleDevelopmentRegion</key><string>en</string>\n"
            "  "
            "<key>CFBundleSupportedPlatforms</key><array><string>%s</"
            "string></array>\n  "
            "<key>DTPlatformName</key><string>%s</string>\n  "
            "<key>LSRequiresIPhoneOS</key><true/>\n  "
            "<key>MinimumOSVersion</key><string>16.0</string>\n"
            "  "
            "<key>UIDeviceFamily</key><array><integer>1</"
            "integer><integer>2</integer></array>\n  "
            "<key>UIRequiresFullScreen</key><true/>\n  "
            "<key>UILaunchScreen</key><dict/>\n  "
            "<key>UIFileSharingEnabled</key><true/>\n"
            "  "
            "<key>NSLocalNetworkUsageDescription</"
            "key><string>Two-player racing with a nearby "
            "device.</string>\n  <key>NSBonjourServices</key><array>\n "
            "   <string>_orion._tcp</string>\n    "
            "<string>_orion._udp</string>\n  </array>\n"
            "  <key>UISupportedInterfaceOrientations</key><array>\n%s  "
            "</array>\n</dict></plist>\n",
            name, name, name, name, version, version,
            sim ? "iPhoneSimulator" : "iPhoneOS",
            sim ? "iphonesimulator" : "iphoneos",
            landscape
                ? "    "
                  "<string>UIInterfaceOrientationLandscapeLeft</"
                  "string>\n    "
                  "<string>UIInterfaceOrientationLandscapeRight</"
                  "string>\n"
                : "    "
                  "<string>UIInterfaceOrientationPortrait</string>\n"));
    if (sim) { // the simulator takes an ad-hoc signature and no profile
        for (int i = 0; i < done.count; i++)
            run_shell(
                format("codesign --force --sign - %s/%s 2>/dev/null",
                       frameworks, (char*)done.data[i]));
        run_shell(
            format("codesign --force --sign - %s 2>/dev/null", app));
        return app;
    }
    const char* udid    = S->device.host;
    char *      profile = 0,
         *team = 0; // the profile that lists this phone; xcode 16 keeps
                    // them under UserData
    const char* dirs[2] = {
        "Library/Developer/Xcode/UserData/Provisioning Profiles",
        "Library/MobileDevice/Provisioning Profiles"};
    for (int i = 0; i < 2 && udid && !profile; i++) {
        char* found = shell_line(format(
            "for p in \"$HOME/%s\"/*.mobileprovision; do security cms "
            "-D -i \"$p\" 2>/dev/null | grep -q %s && echo \"$p\"; "
            "done 2>/dev/null | xargs -I{} stat -f '%%m {}' {} "
            "2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-",
            dirs[i], udid));
        if (*found) {
            profile = found;
            team    = shell_line(
                format("security cms -D -i \"%s\" | plutil -extract "
                             "TeamIdentifier.0 raw -o - -",
                          found));
        }
    }
    if (!profile) {
        fprintf(stderr,
                "[%s] ios: no provisioning profile lists device %s — "
                "bundle unsigned\n",
                name, udid ? udid : "(none)");
        return app;
    }
    run_shell(
        format("cp \"%s\" %s/embedded.mobileprovision", profile, app));
    char* entitlements = format("%s/%s.entitlements", S->out_dir, name);
    run_shell(format("security cms -D -i \"%s\" | plutil -extract "
                     "Entitlements xml1 -o %s -",
                     profile, entitlements));
    char* identity = shell_line(
        format("security find-identity -v -p codesigning 2>/dev/null | "
               "grep 'Apple Development' | grep '%s' | head -1 | sed "
               "'s/.*\"\\(.*\\)\"/\\1/'",
               team ? team : ""));
    if (!*identity)
        identity =
            shell_line("security find-identity -v -p codesigning "
                       "2>/dev/null | grep 'Apple Development' | head "
                       "-1 | sed 's/.*\"\\(.*\\)\"/\\1/'");
    if (!*identity) {
        fprintf(
            stderr,
            "ios: no 'Apple Development' identity in the keychain\n");
        exit(1);
    }
    fprintf(stderr, "[%s] ios: signing as %s (team %s)\n", name,
            identity, team ? team : "");
    for (int i = 0; i < done.count; i++)
        if (run_shell(format(
                "codesign --force --sign \"%s\" %s/%s 2>/dev/null",
                identity, frameworks, (char*)done.data[i]))) {
            fprintf(stderr, "ios: codesign failed\n");
            exit(1);
        }
    if (run_shell(format("codesign --force --sign \"%s\" "
                         "--entitlements %s %s 2>/dev/null",
                         identity, entitlements, app))) {
        fprintf(stderr, "ios: codesign failed for %s\n", app);
        exit(1);
    }
    return app;
}
static int
ios_run(void) { // an iphone installs and launches through
                // devicectl; host is its udid, or a simulator
    const char* name = S->modname;
    const char* host = S->device.host;
    // devicectl; host is its udid, or a simulator
    // name
    char* app = format("%s/%s.app", S->out_dir, name);
    if (access(app, R_OK)) {
        fprintf(stderr, "[%s] ios: no bundle at %s\n", name, app);
        return 1;
    }
    if (strstr(S->platform, "simulator")) {
        if (!same(host, "booted"))
            run_shell(format("xcrun simctl boot %s 2>/dev/null", host));
        run_shell("open -a Simulator");
        if (run_shell(
                format("xcrun simctl install %s %s", host, app))) {
            fprintf(stderr,
                    "[%s] ios: simulator install failed — is one "
                    "booted?\n",
                    name);
            return 1;
        }
        return run_shell(
            format("xcrun simctl launch --console %s com.silver.%s",
                   host, name));
    }
    fprintf(stderr, "[%s] installing on %s\n", name, host);
    if (run_shell(
            format("xcrun devicectl device install app --device %s %s",
                   host, app))) {
        fprintf(stderr,
                "[%s] ios: install failed — is the phone unlocked "
                "and trusted?\n",
                name);
        return 1;
    }
    return run_shell(format("xcrun devicectl device process launch "
                            "--console --device %s com.silver.%s",
                            host, name));
}
