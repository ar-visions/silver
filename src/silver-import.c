

#include <import>
#include <sys/stat.h>
#include <utime.h>
#include <limits.h>

static i64 ancestor_mod = 0;

#if defined(__x86_64__) || defined(_M_X64)
static symbol arch = "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
static symbol arch = "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
static symbol arch = "arm64";
#elif defined(__arm__) || defined(_M_ARM)
static symbol arch = "arm32";
#endif

#if defined(__linux__)
static symbol lib_pre  = "lib"; static symbol lib_ext  = ".so";     static symbol app_ext  = "";        static symbol platform = "linux";
#elif defined(_WIN32)
static symbol lib_pre  = "";    static symbol lib_ext  = ".dll";    static symbol app_ext  = ".exe";    static symbol platform = "windows";
#elif defined(__APPLE__)
static symbol lib_pre  = "lib"; static symbol lib_ext  = ".dylib";  static symbol app_ext  = "";        static symbol platform = "darwin";
#endif

static bool is_dbg(import t, string query, cstr name, bool is_remote) {
    cstr  dbg = (cstr)query->chars; // getenv("DBG");
    char  dbg_str[PATH_MAX];
    char name_str[PATH_MAX];
    sprintf(dbg_str, ",%s,", dbg);
    sprintf(name_str, "%s,", name_str);
    int   name_len    = strlen(name);
    int   dbg_len     = strlen(dbg_str);
    int   has_astrick = 0;
    
    for (int i = 0, ln = strlen(dbg_str); i < ln; i++) {
        if (dbg_str[i] == '*') {
            if (dbg_str[i + 1] == '*')
                has_astrick = 2;
            else if (has_astrick == 0)
                has_astrick = 1;
        }
        if (strncmp(&dbg_str[i], name, name_len) == 0) {
            if (i == 0 || dbg_str[i - 1] != '-')
                return true;
            else
                return false;
        }
    }
    bool is_local = !is_remote;
    return has_astrick > 1 || 
           (has_astrick && has_astrick == (int)is_local);
}

none sync_tokens(import t, path build_path, string name) {
    path t0 = form(path, "%o/import-token", build_path);
    path t1 = form(path, "%o/tokens/%o", t->install, name);
    struct stat build_token, installed_token;
    /// create token pair (build & install) to indicate no errors during config/build/install
    cstr both[2] = { cstring(t0), cstring(t1) };
    for (int i = 0; i < 2; i++) {
        FILE* ftoken = fopen(both[i], "wb");
        fwrite("im-a-token", 10, 1, ftoken);
        fclose(ftoken);
    }
    int istat_build   = stat(cstring(t0), &build_token);
    int istat_install = stat(cstring(t1), &installed_token);
    struct utimbuf times;
    times.actime  = build_token.st_atime;  // access time
    times.modtime = build_token.st_mtime;  // modification time
    utime(cstring(t1), &times);
}

string import_config_string(import a) {
    string config = string(alloc, 128);
    each (a->config, string, conf) {
        if (starts_with(conf, "-l"))
            continue;
        if (len(config))
            append(config, " ");
        concat(config, conf);
    }
    return config;
}

string flag_cast_string(flag a) {
    string res = string(alloc, 64);

    if (a->is_cflag)
        concat(res, a->name);
    else {
        if (a->is_static)
            append(res, "-Wl,-Bstatic ");
        concat(res, form(string, "-l%o", a->name));
        if (a->is_static)
            append(res, " -Wl,-Bdynamic");
    }
    return res;
}

none add_flag(import a, array list, line l, map environment) {
    each (l->text, string, w) {
        string directive = get(environment, string("DIRECTIVE"));
        string t = evaluate(w, environment); // PROJECT is missing null char
        if (len(t)) {
            //print("%o: %o", top_directive, t);
            bool is_lib = false;
            static cstr linker_labels[] = {
                "-l", "--sysroot=", "-isystem ",
                "-Wl,-rpath,", "-nodefaultlibs",
                "-static"
            };
            for (int i = 0; i < sizeof(linker_labels) / sizeof(cstr); i++) {
                if (starts_with(t, linker_labels[i])) {
                    is_lib = true;
                    break;
                }
            }
            bool is_cflag  = !is_lib && t->chars[0] == '-';
            bool is_static = t->chars[0] == '@';
            push(list, flag(name,
                    is_static ? mid(t, 1, len(t) - 1) : t,
                is_static, is_static, is_cflag, is_cflag, is_lib, is_lib));
        }
    }
}

// tapestry with map
import import_with_map(import a, map m) {


    /// this method mainly parses the import file (and we are reorganizing this)
    if (lines)
        each(lines, line, l) {
            string first = get(l->text, 0);
            i32    flen  = len(first);
            
            if (first(first) == '#') continue;

            /// handle import/lib/app/test (must be indent 0)
            if (l->indent == 0) {
                verify(first->chars[flen - 1] == ':', "expected base-directive and ':'");
                last_directive = mid(first, 0, flen - 1);
                verify(cmp(last_directive, "import") == 0 ||
                    cmp(last_directive, "export") == 0, "expected import or export directive");
                is_import = cmp(last_directive, "import") == 0;
                top_directive = copy(last_directive);
                set(environment, string("DIRECTIVE"),
                    is_import ? string("src") : 
                    (parent ? parent->has_lib : a->has_lib) ? string("src") : // must be based on parent if set
                                string("app"));

            } else if (first->chars[flen - 1] == ':') {
                /// independent of indent, up to user in how to construct
                string n = mid(first, 0, flen - 1);
                if (cmp(n, "linux") == 0 || cmp(n, "windows") == 0 || cmp(n, "darwin") == 0)
                    last_platform = n;
                else
                    last_arch = n;
            } else if (l->indent == 1) {
                //
            } else if (!parent) {
                if ((!last_platform || cmp(last_platform, platform) == 0) &&
                    (!last_arch     || cmp(last_arch,     arch)     == 0)) {
                    bool use_command = cmp(first, ">") == 0;
                    if (use_command || cmp(first, "!") == 0) {
                        // combine tokens into singular string
                        string cmd = string(alloc, 64);
                        each(l->text, string, w) {
                            if (first == w) continue;
                            if (len(cmd))
                                append(cmd, " ");
                            concat(cmd, w);
                        }
                        push(use_command ? im->commands : im->always, cmd);
                    } else {
                        each(l->text, string, w) {
                            /// if its VAR=VAL then this is not 'config' but rather environment
                            char f = w->chars[0];
                            cstr e = strstr(w->chars, "=");
                            if (isalpha(f) && (f >= 'A' && f <= 'Z') && e) {
                                char name[128];
                                sz   ln = (sz)(e - w->chars);
                                verify(ln < 128, "invalid data");
                                memcpy(name, w->chars, ln);
                                name[ln] = 0;
                                char value[128];
                                sz value_ln = len(w) - ln - 1;
                                memcpy(value, &w->chars[ln + 1], value_ln);
                                string n   = trim(string(name));
                                string val = evaluate(string(value), environment);
                                set(im->environment, n, val);
                            } else {
                                string e = evaluate(w, environment);
                                push(im->config, e);
                            }
                        }
                    }
                }
            }
        }


    return a;
}

string import_cmake_location(import im) {
    int index = 0;
    each (im->config, string, conf) {
        if (starts_with(conf, "-S")) {
            return form(string, "%o %o", conf, im->config->elements[index + 1]);
        }
        index++;
    }
    return null;
}

string serialize_environment(map environment, bool b_export);

/// handle autoconfig (if present) and resultant / static Makefile
bool import_make(import im) {
    import t = im->import;
    path install = copy(t->install);
    i64 conf_status = INT64_MIN;
    path t0 = form(path, "silver-token");
    path t1 = form(path, "%o/tokens/%o", install, im->name);
    path checkout = form(path, "%o/checkout", install);

    make_dir(im->build_path);
    cd(im->build_path);
    string env = serialize_environment(im->environment, false);
    
    if (file_exists("%o", t0) && file_exists("%o", t1)) {
        /// get modified date on token, compare it to one in install dir
        i64 token0 = modified_time(t0);
        i64 token1 = modified_time(t1);

        if (token0 && token1) {
            conf_status = abs((i32)(token0 - token1));
            if (conf_status < 1000)
                conf_status = 0;
        }
        if (conf_status == 0) {
            i64 latest = 0;
            path latest_f = latest_modified(im->import_path, &latest);
            if (latest > token0 || ancestor_mod > token0)
                conf_status = -1;
        }
    }

    if (conf_status == 0) {
        clear(im->commands);
        print("%22o: build-cache", im->name);
        return true;
    }

    string cmake_conf = cmake_location(im);
    string args = import_config_string(im);
    cstr debug_r = im->debug ? "debug" : "release";
    setenv("BUILD_CONFIG", args->chars, 1);
    setenv("BUILD_TYPE", debug_r, 1);

    if (file_exists("../Cargo.toml")) {
        // todo: copy bin/lib after
        verify(exec("cargo build --%s --manifest-path ../Cargo.toml --target-dir .",
            im->debug ? "debug" : "release") == 0, "rust compilation");
    } else if (cmake_conf || file_exists("../CMakeLists.txt")) {
        /// configure
        if (!file_exists("CMakeCache.txt")) {
            cstr build = im->debug ? "Debug" : "Release";
            int  iconf = exec(
                "cmake -B . -S .. -DCMAKE_INSTALL_PREFIX=%o -DCMAKE_BUILD_TYPE=%s %o", install, build, im);
            verify(iconf == 0, "%o: configure failed", im->name);
        }
        /// build & install
        int    icmd = exec("%o cmake --build . -j16", env);
        int   iinst = exec("%o cmake --install .",   env);

    } else {
        cstr Makefile = "Makefile";
        /// build for A-type projects
        if (file_exists("../%s", Makefile) && file_exists("../build.sf", Makefile)) {
            cd(im->import_path);
            int imake = exec("%o make", env);
            verify(imake == 0, "make");
            cd(im->build_path);
        } else if (!file_exists("Makefile")) {
            cd(im->import_path);
            /// build for automake projects
            if (file_exists("./autogen.sh")  || 
                file_exists("./configure.ac") || 
                file_exists("./configure")    ||
                file_exists("./config")) {
                
                // this is not libffi at all, its a race condition that it and many others have with autotools
                if (!file_exists("./ltmain.sh"))
                    verify(exec("libtoolize --install --copy --force") == 0, "libtoolize");
                
                // neither this -- but its a common preference on these repos
                if (file_exists("./autogen.sh")) {
                    verify(exec("bash ./autogen.sh") == 0, "autogen");
                }
                /// generate configuration scripts if available
                else if (!file_exists("./configure") && file_exists("./configure.ac")) {
                    verify(exec("autoupdate .")    == 0, "autoupdate");
                    verify(exec("pwd") == 0, "autoreconf");
                    verify(exec("autoreconf -i .") == 0, "autoreconf");
                }
                /// prefer our pre/generated script configure, fallback to config
                cstr configure = file_exists("./configure") ? "./configure" : "./config";
                if (file_exists("%s", configure)) {
                    verify(exec("%o %s%s --prefix=%o %o",
                        env,
                        configure,
                        im->debug ? " --enable-debug" : "",
                        install,
                        im) == 0, configure);
                }
            }
            if (file_exists("%s", Makefile))
                verify(exec("%o make -f %s install", env, Makefile) == 0, "make");
        }
    }
    return true;
}

static array headers(path dir) {
    array all = ls(dir, null, false);
    array res = array();
    each (all, path, f) {
        string e = ext(f);
        if (len(e) == 0 || cmp(e, ".h") == 0)
            push(res, f);
    }
    drop(all);
    return res;
}

static int filename_index(array files, path f) {
    string fname = filename(f);
    int    index = 0;
    each(files, path, p) {
        string n = filename(p);
        if (compare(n, fname) == 0)
            return index;
        index++;
    }
    return -1;
}

static bool sync_symlink(path src, path dst) {
    return false;
}

/// install headers, then overlay built headers; then install libs and app targets
i32 import_install(import a) {
    path   install      = a->install;
    path   install_inc  = form(path, "%o/include", install);
    path   install_lib  = form(path, "%o/lib",     install);
    path   install_app  = form(path, "%o/bin",     install);
    path   project_lib = form(path, "%o/src", a->project_path);
    path   build_lib   = form(path, "%o/src", a->build_path);
    array  project_h   = headers(project_lib);
    array  build_h     = headers(build_lib);

    /// no headers are copied, just the directory symlinked for the src lib
    /// app headers are known by -I already, and apps access <appname-import> directly
    if (dir_exists("%o", project_lib)) {
        path f   = f(path, "%o/src", a->build_path);
        path dst = f(path, "%o/%o", install_inc, a->name);
        exec("rm -rf %o",      dst);
        exec("ln -s %o %o", f, dst);
    }
    /*
    each(project_h, path, f) {
        string fname = filename(f);
        if (filename_index(build_h, f) < 0) {
            path dst = f(path, "%o/%o", install_inc, fname);
            if (!eq(f, dst)) {
                exec("rm -rf %o",   dst);
                exec("ln -s %o %o", f, dst);
            }
            //exec("rsync -a %o %o/", f, install_inc);
        }
    }
    */

    each (build_h, path, f) {
        string  fname = filename(f);
        if (!eq(fname, "import")) {
            path dst = f(path, "%o/%o", install_inc, fname);
            if (!eq(f, dst)) {
                exec("rm -rf %o",   dst);
                exec("ln -s %o %o", f, dst);
            }
            //exec("rsync -a %o %o/", f, install_inc);
        }
    }

    each(a->lib_targets, path, lib) {
        path dst = f(path, "%o/%o", install_lib, filename(lib));
        if (!eq(lib, dst)) {
            exec("rm -rf %o",   dst);
            exec("ln -s %o %o", lib, dst);
        }
        //exec("rsync -a %o %o/%o", lib, install_lib, filename(lib));
    }

    each(a->app_targets, path, app) {
        path dst = f(path, "%o/%o", install_app, filename(app));
        if (!eq(app, dst)) {
            exec("rm -rf %o",   dst);
            exec("ln -s %o %o", app, dst);
        }
        //exec("rsync -a %o %o/%o", app, install_app, filename(app));
    }

    return 0;
}

none cflags_libs(import im, string* cflags, string* libs) {
    *libs   = string(alloc, 64);
    *cflags = string(alloc, 64);

    array lists[2] = { im->interns, im->exports };
    for (int i = 0; i < 2; i++)
        each (lists[i], flag, fl) {
            print("%o: %o", im->name, fl->name);
            if (fl->is_cflag) {
                concat(*cflags, cast(string, fl));
                append(*cflags, " ");
            } else if (fl->is_lib) {
                concat(*libs, form(string, "%o", fl->name));
                append(*libs, " ");
            }
        }
    
    if (im->exports)
        each (im->exports, flag, fl) {
            if (fl->is_cflag) {
                concat(*cflags, cast(string, fl));
                append(*cflags, " ");
            } else if (fl->is_lib) {
                concat(*libs, form(string, "%o", fl->name));
                append(*libs, " ");
            }
        }
    
    bool has_lib = false;
    each (im->config, string, conf) {
        if (starts_with(conf, "-l")) {
            concat(*libs, conf);
            append(*libs, " ");
            has_lib = true;
        }
    }
    if (!has_lib) {
        concat(*libs, form(string, "-l%o", im->name));
        append(*libs, " ");
    }
}

static bool is_checkout(path a) {
    path par = parent(a);
    string st = stem(par);
    if (eq(st, "checkout")) {
        return true;
    }
    return false;
}



none import_link_shares(import a, path project_from) {
    if (!dir_exists("%o/share", project_from))
        return;
    // if this is debug, we want to rsync everything
    path import_share = f(path, "%o/share", project_from);
    array dir = ls(import_share, null, false); // no pattern, and recursion set
    each (dir, path, share_folder) {
        // verify that its indeed a folder, and not a file resource (not supported at root level)
        verify(is_dir(share_folder), "unsupported file structure");
        string rtype = filename(share_folder);
        array rfiles = ls(share_folder, null, false);

        path rtype_dir  = f(path, "%o/share/%o/%o", a->install, a->name, rtype);
        make_dir(rtype_dir);

        each (rfiles, path, res) {
            // create symlink at dest  install/share/our-target-name/each-resource-dir/each-file -> im->import_path/share/each-resource-dir/each-resource-file
            string fn = filename(res);
            path src = f(path, "%o/share/%o/%o", project_from, rtype, fn);
            path dst = f(path, "%o/share/%o/%o/%o", a->install, a->name, rtype, fn);
            if (file_exists("%o", dst) && !is_symlink(dst))
                continue; // being used by the user (needs an option for release/packaging mode here)
            bool needs_link = !eq(src, dst);
            
            if (needs_link) {
                exec("rm -rf %o", dst);
                verify(!file_exists("%o", dst), "cannot create symlink");
                exec("ln -s %o %o", src, dst);
            }
        }
    }
}

// build with optional bc path; if no bc path we use the project file system
i32 import_build(import a, path bc) {
    int  error_code = 0;
    bool debug = is_dbg(a, a->dbg, (cstr)a->name->chars, false);
    bool sanitize = debug && is_dbg(a, a->sanitize, (cstr)a->name->chars, false); // sanitize does not work with musl, and perhaps we need a special linking area for sanitize; bit lame!

    if (bc) {
        // simplified process for .bc case
        string name = stem(bc);
        verify(exec("%o/bin/llc -filetype=obj %o.ll -o %o.o -relocation-model=pic",
            a->install, name, name) == 0,
                ".ll -> .o compilation failed");
        string libs, cflags;
        cflags_libs(a, &cflags, &libs); // fetches from all exported data
        verify(exec("%o/bin/clang %o.o -o %o -L %o/lib %o %o",
            a->install, name, name, a->install, libs, cflags) == 0,
                "link failed");
        return 0;
    }

    cd(a->project_path);
    AType type = isa(a->build_path);
    make_dir(a->build_path);

    path   project_lib  = form  (path, "%o/src",  a->project_path);
    path   project_app  = form  (path, "%o/app",  a->project_path);
    path   build_lib    = form  (path, "%o/src",  a->build_path);
    path   build_app    = form  (path, "%o/app",  a->build_path);
    path   build_test   = form  (path, "%o/test", a->build_path);
    cstr   CC           = getenv("CC");       if (!CC)       CC       = "clang";
    cstr   CXX          = getenv("CXX");      if (!CXX)      CXX      = "clang++";
    cstr   CFLAGS       = getenv("CFLAGS");   if (!CFLAGS)   CFLAGS   = "";
    cstr   CXXFLAGS     = getenv("CXXFLAGS"); if (!CXXFLAGS) CXXFLAGS = "";
    string name         = filename(a->project_path);
    string n2           = copy(name);
    bool   cpp          = false;
    cstr   compilers[2] = { CC, CXX };
    path   include      = form(path, "%o/include", a->install);
    a->lib_targets      = array();
    a->app_targets      = array();
    bool has_lib        = dir_exists("%o", project_lib);
    path project_main   = has_lib ? project_lib : project_app;

    struct dir {
        cstr  dir;
        cstr  output_dir;
        path  build_dir;
    } dirs[3] = {
        { "src",  "src",  build_lib  },
        { "app",  "bin",  build_app  },
        { "test", "test", build_test } // not allowed if no lib
    };

    for (int i = 0; i < sizeof(dirs) / sizeof(struct dir); i++) {
        struct dir* flags = &dirs[i];
        bool  is_lib    = strcmp(flags->dir, "src") == 0;
        path  dir       = form(path, "%o/%s", a->project_path, flags->dir);
        path  lib_src   = form(path, "%o/%s", a->project_path, "src");
        path  build_dir = form(path, "%o/%s", a->build_path, flags->output_dir);
        path  build_dir2 = form(path, "%o/%o", a->build_path, flags->build_dir);
        make_dir(flags->build_dir);
        make_dir(build_dir);
        make_dir(build_dir2);
        if (!dir_exists("%o", dir)) continue;

        string base_cflags = string(""), base_libs = string("");
        /// if its a stand-alone app or has a lib, it should get all of the exports
        /// also lib gets everything (i == 0)
        if (has_lib && (i == 0) || !has_lib) {
            cflags_libs(a, &base_cflags, &base_libs);
        }
        /// app and test depend on its lib
        if (has_lib && i != 0)
            base_libs = form(string, "%o -l%o", base_libs, name);

        cd(flags->build_dir);

        bool is_app  = (i == 1);

        array c      = ls(dir, string(".c"),   false); // returns absolute paths
        array cc     = ls(dir, string(".cc"),  false);
        cpp         |= len(cc) > 0;
        array obj    = array(64);
        array obj_c  = array(64);
        array obj_cc = array(64);
        path  href   = form(path, "%o/%o", dir, name);
        array files  = ls(dir, null, false);
        i64   htime  = 0;

        if (!file_exists("%o", href))
             href = form(path, "%o/%o.h", dir, name);
    
        /// get latest header modified (including project header)
        each (files, path, f) {
            string e = ext(f);
            string s = stem(f);
            if (cmp(s, "h") == 0 || (len(e) == 0 && cmp(s, name->chars) == 0)) {
                i64 mt = modified_time(f);
                if (mt > htime)
                    htime = mt;
            }
        }
        string cflags = string(alloc, 64);
        string cxxflags = string(alloc, 64);

        /// compile C with cflags
        struct lang {
            array source;
            array objs;
            cstr  compiler;
            cstr  std;
        } langs[2] = {
            {c,  obj_c,  compilers[0], "gnu11"},
            {cc, obj_cc, compilers[1], "c++17"}
        };
        i64 latest_o = 0;
        for (int l = 0; l < 2; l++) {
            struct lang* lang = &langs[l];
            if (!lang->std) continue;
            string compiler = form(string, "%s -c %s%s-std=%s %s %o %o -I%o -I%o -I%o -I%o",
                lang->compiler, debug ? "-g2 " : "", sanitize ? "-fsanitize=address " : "", lang->std,
                l == 0 ? CFLAGS : CFLAGS, /// CFLAGS from env come first
                base_cflags,
                l == 0 ? cflags : cflags, /// ... then project-based flags
                (has_lib && i == 2) ? build_lib : flags->build_dir,
                dir, lib_src, include); /// finally, an explicit -I of our directive
            
            /// for each source file, make objects file names, and compile if changed
            each(lang->source, path, src) {
                string module = filename(src);
                string module_name = stem(src);
                string o_file = form(string, "%o.o",  module);
                path   o_path = form(path,   "%o/%o", a->build_path, o_file);
                i64    o_modified  = modified_time(o_path);
                latest_o = max(latest_o, o_modified);
                push(lang->objs, o_path);
                push(obj, o_path);

                /// recompile if newer / includes differ
                i64 mtime = modified_time(src);
                bool source_newer  = (mtime > o_modified) || (ancestor_mod && (ancestor_mod < mtime)); // || ; /// | project_rebuild (based on newest project we depend on)
                bool header_change = htime && htime > modified_time(o_path);
                if (source_newer || header_change) {
                    int compile = exec("%o -DMODULE=\"\\\"%o\\\"\" %o -o %o", compiler, module_name, src, o_path);
                    verify(compile == 0, "compilation");
                    latest_o = max(latest_o, modified_time(o_path));
                }
            }
        }

        // link
        string lflags = string(alloc, 64);
        //concat(lflags, form(string, "-L%o/lib ", a->build_path));
        concat(lflags, form(string, "-L%o/lib -Wl,-rpath,%o/lib%s", a->install, a->install,
            sanitize ? " -fsanitize=address " : ""));

        if (is_lib) {
            path output_lib = form(path, "%o/lib/%s%o%s", a->build_path, lib_pre, n2, lib_ext);
            path install_lib = form(path, "%o/lib/%s%o%s", a->install, lib_pre, n2, lib_ext);
            if (!latest_o || (modified_time(install_lib) < latest_o)) {
                verify (exec("%s -shared %o %o %o -o %o",
                    cpp ? CXX : CC,
                    lflags, base_libs, obj, output_lib) == 0, "linking");
            }
            // install lib right away
            if (true || debug) {
                if (!eq(output_lib, install_lib)) {
                    exec("rm -rf %o",   install_lib);
                    exec("ln -s %o %o", output_lib, install_lib);
                }
            } else
                exec("rsync -a %o %o", output_lib, install_lib);
            
            push(a->lib_targets, output_lib);
        } else {
            each (obj_c, path, obj) {
                string module_name = stem(obj);
                path output = form(path, "%o/%s/%o%s", a->build_path, flags->output_dir, module_name, app_ext);
                if (modified_time(output) < modified_time(obj)) {
                    verify (exec("%s %o %o %o -o %o",
                        CC, lflags, base_libs, obj, output) == 0, "linking");
                }
                if (is_app)
                    push(a->app_targets, output);
                else
                    drop(output);
                drop(module_name);
            }
            each (obj_cc, path, obj) {
                string module_name = stem(obj);
                path output_o = form(path, "%o/%s/%o", a->build_path, flags->output_dir, module_name);
                if (modified_time(output_o) < modified_time(obj)) {
                    verify (exec("%s %o %o %o -o %o",
                        CXX, lflags, obj, output_o) == 0, "linking");
                }
                drop(module_name);
                drop(output_o);
            }
            // run test here, to verify prior to install; will need updated PATH so they may run the apps we built just prior to test
        }
    }

    // Check if we have headers to process
    if (dir_exists("%o", project_lib)) {
        // Generate Silver format from concatenated headers
        path silver_output = form(path, "%o/include/%o.sf", a->install, name);
        path main_header = form(path, "%o/%o", project_lib, name);
        
        // Check if we need to regenerate (if any header is newer than .sf file)
        i64 silver_time = modified_time(silver_output);
        i64 latest_header_time = 0;
        
        // Check main header
        if (file_exists("%o", main_header)) {
            latest_header_time = max(latest_header_time, modified_time(main_header));
        }
        
        // Check all .h files
        array h_files = ls(project_lib, string(".h"), false);
        each(h_files, path, h_file) {
            latest_header_time = max(latest_header_time, modified_time(h_file));
        }
        
        // Regenerate if needed
        if (latest_header_time > silver_time || !file_exists("%o", silver_output)) {
            //print("%22o: generating silver format", name);
            //int convert_result = exec("python3 %o/c-silver.py --concat %o %o %o",
            //    a->install, project_lib, name, silver_output);
            //verify(convert_result == 0, "Silver format generation failed");
        }
        
        drop(h_files);
    }
    
    // for each import with a share folder in its repo, symlink all files individually (cannot use folders safely because our targets stack resources)
    import_link_shares(a, a->project_path);

    /// now we set the token here! (we do this twice; the import layer does it 
    // (and CAN be skipped if its a import project -- should be without side effect, though, since its immediately after))
    sync_tokens(a, a->build_path, a->name);
    return error_code;
}

import import_with_silver(import im, silver mod) {
    
}

/// initialization path
none import_init(import a) {
    a->name = filename(a->project_path);




    if (!parent) {
        string text0 = get(l->text, 0);
        string name   = evaluate(text0, environment);
        if (name->chars[0] == '-') {
            /// we want our main target (lib or app, if isolated) to get these cflag or libs
            /// its the same as 'export' section, but not exported
            add_flag(a, a->interns, l, environment);
        } else {
            verify(is_import, "expected import");
            string uri    = evaluate((string)get(l->text, 1), environment);
            string commit = evaluate((string)get(l->text, 2), environment);
            if (len(s_imports))
                append(s_imports, " ");
            concat(s_imports, name);
            /// currently its the headers.sh
            im = allocate(remote,
                import, a,
                name,   name,
                uri,    uri,
                commit, commit,
                environment, map(),
                config, array(64), commands, array(16), always, array(16));
            push(a->remotes, im);
        }

        last_platform = null;
        last_arch = null;
    } else if (!is_import) {
        add_flag(a, a->exports, l, environment);
    }





    // was remote:
    path   cwdir   = cwd();
    path   install = copy(im->import->install);
    path   src     = im->import->src;
    bool   is_remote = !dir_exists("%o/%o", src, im->name);
    path checkout     = f(path, "%o/checkout", install);
    im->import_path   = f(path, "%o/%o", checkout, im->name);
    
    if (!dir_exists("%o/%o", checkout, im->name)) {
        cd(checkout);
        if (len(src) && dir_exists("%o/%o", src, im->name)) {
            verify (exec("ln -s %o/%o %o/%o",
                src, im->name, checkout, im->name) == 0, "symlink");
            is_remote = false;
        } else {
            exec("rm -rf %o", im->name);
            int clone = exec("git clone %o %o --no-checkout && cd %o && git checkout %o && cd ..",
                im->uri, im->name, im->name, im->commit);
            verify (clone == 0, "git clone");
            if (file_exists("%o/diff/%o.patch", im->import->project_path, im->name)) {
                cd(im->import_path);
                verify(exec("git apply %o/diff/%o.patch", im->import->project_path, im->name) == 0, "patch");
                cd(checkout);
            }
        }
    }
    im->debug = is_dbg(im->import, im->import->dbg, (cstr)im->name->chars, is_remote);
    symbol build_type = im->debug ? "debug" : "release";
    im->build_path    = f(path, "%o/%s", im->import_path, build_type);
    string n = im->name;
    i32* c = null, *b = null, *i = null;
    if (file_exists("%o/build.sf", im->import_path)) {
        path af_remote = form(path, "%o/build.sf", im->import_path);
        map m_copy = copy(im->import->m);
        set(m_copy, string("parent"), im->import);
        set(m_copy, string("path"), af_remote);
        import t_remote = import(m_copy);
        im->exports = hold(t_remote->exports);
        i64 mod = modified_time(f(path, "%o/tokens/%o", im->import->install, im->name));
        if (mod && (!ancestor_mod || ancestor_mod < mod))
             ancestor_mod = mod;
        drop(t_remote);
    }
    make_dir(im->build_path);
    cd(im->build_path);
    make(im);
    cd(im->build_path);
    path   cwd_after = cwd();
    each(im->commands, string, cmd) {
        print("> %o: command: %o", im->name, cmd);
        verify(exec("%o", evaluate(cmd, im->import->environment)) == 0, "expected successful command post-install");
    }
    each(im->always, string, cmd) {
        print("! %o: command: %o", im->name, cmd);
        verify(exec("%o", evaluate(cmd, im->import->environment)) == 0, "expected successful ! inline command");
    }
    sync_tokens(im->import, im->build_path, im->name);
    cd(cwdir);












    import parent       = get(m, string("parent"));
    path af               = get(m, string("path"));
    a->m                  = hold(m);
    a->dbg                = hold(get(m, string("dbg")));
    a->sanitize           = hold(get(m, string("sanitize")));
    a->install            = hold(get(m, string("install")));
    a->src                = hold(get(m, string("src")));

    a->project_path       = directory(af);
    a->name               = filename(a->project_path);
    cstr build_dir        = is_dbg(a, a->dbg, (cstr)a->name->chars, false) ? "debug" : "release";
    a->build_path         = form(path, "%o/%s", a->project_path, build_dir);
    a->exports            = array(64);
    a->interns            = array(64);
    a->environment        = map(assorted, true);
    path build_file       = file_exists("%o", af) ? af :
         (dir_exists("%o",  af) && file_exists("%o/build.sf", af)) ? 
        f(path, "%o/build.sf", af) : null; 
    array  lines          = build_file ? read(af, typeid(array), null) : null;
    remote im             = null;
    string last_arch      = null;
    string last_platform  = null;
    string last_directive = null;
    string top_directive  = null;
    bool   is_import      = true;
    string s_imports      = string(alloc, 64);
    path   project_lib    = form(path, "%o/src",  a->project_path);
    a->has_lib            = dir_exists("%o", project_lib); /// and contains source?

    /// for each line, process by indentation level, and evaluate tokens
    /// when exports are evaluated by a parent, the environment but reflect their own
    set(a->environment, string("PROJECT"),      a->name);
    set(a->environment, string("PROJECT_PATH"), a->project_path);
    set(a->environment, string("BUILD_PATH"),   a->build_path);
    set(a->environment, string("IMPORTS"),      s_imports);
     
    map environment = parent ? parent->environment : a->environment;








}


define_class(import,    A)
define_class(flag,      A)
