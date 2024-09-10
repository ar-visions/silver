#include <silver>

int main(int argc, char **argv) {
    A_start();
    AF     pool     = allocate(AF);
    map    defaults = map_of(
        "module",  str(""),
        "install", new(path, chars, "/home/kalen/src/silver-import"), null);
    print("defaults = %o", defaults);

    string ikey     = str("install");
    map    args     = A_args(argc, argv, defaults, ikey);
    print("args = %o", args);

    path   install  = M(args, get, ikey);
    string mkey     = str("module");
    string name     = M(args, get, mkey);
    path   n        = new(path, chars, name->chars);
    path   source   = M(n, absolute);

    assert (M(source, exists), "source %o does not exist", n);
    
    silver module   = new(silver, source, source, install, install);
    drop(pool);
}