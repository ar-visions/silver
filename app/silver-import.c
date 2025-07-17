
#include <silver-import/import>

#undef link

#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utime.h>

void on_signal(int sig) {
    pid_t pid = last_pid();
    printf("\nimport: signal %i\n", sig);
    if (pid > 0)
        killpg(pid, SIGTERM);
    exit(1);
} 

int main(int argc, cstr argv[]) {
    // this is useful for apps built in debug-mode
    // debug is for development mode, so lets add features to make development easier
    
    // heres one:
    // know thy-self (this should be integral to A_start; for that though, we need artifact tracing)
    char    rpath[4096];
    ssize_t ln = readlink("/proc/self/exe", rpath, sizeof(rpath) - 1);
    struct  stat bin_stat;
    if (ln != -1) {
        rpath[ln] = '\0';
        stat(rpath, &bin_stat);  // reliably points to the running binary
    } else
        perror("readlink failed");
    
    verify(stat(rpath, &bin_stat) == 0, "stat failed on self");
    
    startup(argv);

    signal(SIGINT,  on_signal);  // ctrl+c
    signal(SIGTERM, on_signal);  // kill
    signal(SIGHUP,  on_signal);  // terminal closed

    /// make import feel like a script.. or, cause chaos
    setenv("VERBOSE", "1", true);
    cstr _IMPORT = getenv("IMPORT");
    char* relics[] = { "app/silver-import.c", "src/import.c" }; // this is the trick, of having a list of relics; they could span all dependencies, too (which slows down startup)
    i64  src_mod = 0;
    for (int i = 0; i < sizeof(relics) / sizeof(char*); i++) {
        struct  stat src_stat;
        path relic_path = form(path, "%s/%s", _IMPORT, relics[i]);
        cstr rpath2 = cstring(relic_path);
        if (stat(rpath2, &src_stat) == 0)
            if (src_mod > src_stat.st_mtime)
                src_mod = src_stat.st_mtime;
    }
    if (false && src_mod > bin_stat.st_mtime) {
        char   cwd[2048];
        getcwd(cwd, sizeof(cwd));
        chdir(_IMPORT);
        /// run make, which replaces import binary
        if (exec("make") == 0) {
            /// touch our binary so that its at a later date than the source, just so we can be sure!
            chdir(cwd);
            utime(rpath, NULL); // ensure we will not fall into same trap; or handle time changes
            return exec("%s", rpath); 
        } else {
            print("could not rebuild import, proceeding with this working state...");
        }
    }

    cstr  _DBG            = getenv("DBG");
    cstr  _ASAN           = getenv("ASAN");
    path  default_path    = form  (path, "%s", ".");
    path  default_install = form  (path, "%s", _IMPORT ? _IMPORT : ".");
    map   a               = args(argv,
        "path",    default_path,
        "install", default_install,
        null);

    path  path_unrel      = get (a, string("path"));
    path  install_unrel   = get (a, string("install"));

    string dbg      = _DBG ? string(_DBG) : string("");
    path   install  = absolute(install_unrel);
    path   src      = parent(install);

    print("install is set to %o, and src for sym-linking is %o", install, src);
    
    path   loc      = absolute(path_unrel);
    path   f        = null;

    make_dir(form(path, "%o",           install));
    make_dir(form(path, "%o/lib",       install));
    make_dir(form(path, "%o/bin",       install));
    make_dir(form(path, "%o/tokens",    install));

    if (len(loc))
        cd(loc);
    if (dir_exists("%o", loc) && file_exists("%o/build.sf", loc))
        f  = form(path, "%o/build.sf", loc);
    else if (file_exists("%o", loc))
        f  = form(path, "%o", loc);
    else
        fault("import: cannot build from path[ %o ]", loc);
    path     af = absolute(f);
    map      m  = map_of(
        "path",    af,
        "sanitize", _ASAN ? string(_ASAN) : string(""),
        "dbg",     _DBG ? string(_DBG) : string(""),
        "install", absolute(install_unrel),
        "src",     parent(install), null);
    import t  = import(m);
    i32 build_code   = build  (t, null); if (build_code   != 0) return build_code;
    i32 install_code = install(t);       if (install_code != 0) return install_code;
    print("success");
    return 0;
}
