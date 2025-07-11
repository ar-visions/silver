#include <app/import>

int main(int argc, char **argv) {
    A_start(argv);
    num         types_len;
    A_f**       types    = A_types(&types_len);
    const num   max_args = 8;
    map         args     = A_args(argv,
        "module",  string(""),
        "install", form(path, "%s", getenv("IMPORT")), null);
    string mkey     = string("module");
    string name     = get(args, string("module"));
    path   n        = path(chars, name->chars);
    path   source   = absolute(n);
    silver mod      = silver(
        source,  source,
        install,  get(args, string("install")),
        name,    stem(source));
    return 0;
}