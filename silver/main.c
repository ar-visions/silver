#include <silver>

int main(int argc, char **argv) {
    A_start();
    AF         pool = allocate(AF);
    cstr        src = getenv("SRC");
    cstr     import = getenv("SILVER_IMPORT");
    map    defaults = map_of(
        "module",  str(""),
        "install", import ? form(path, "%s", import) : 
                            form(path, "%s/silver-import", src ? src : "."),
        null);
    print("defaults = %o", defaults);

    string ikey     = str("install");
    map    args     = A_args(argc, argv, defaults, ikey);
    print("args = %o", args);

    path   install  = get(args, ikey);
    string mkey     = str("module");
    string name     = get(args, mkey);
    path   n        = new(path, chars, name->chars);
    path   source   = call(n, absolute);

    assert (call(source, exists), "source %o does not exist", n);
    
    silver module   = new(silver, source, source, install, install);
    drop(pool);
    // we only use silver to build the library for .c, of course.  this was our use-case
}