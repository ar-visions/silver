#ifndef _PWD_H_
#define _PWD_H_
#ifndef AU_EXPORT
#ifdef _WIN32
#define AU_EXPORT __attribute__((dllexport))
#else
#define AU_EXPORT
#endif
#endif

#ifdef _WIN32

// windows keeps no password database. this is the shape a posix
// program expects; the lookups report no such user, which is what
// callers already handle (mtools' floppyd is the only one that asks)

#ifdef __cplusplus
extern "C" {
#endif

struct passwd {
    char* pw_name;
    char* pw_passwd;
    int   pw_uid;
    int   pw_gid;
    char* pw_gecos;
    char* pw_dir;
    char* pw_shell;
};

AU_EXPORT struct passwd* getpwnam(const char* name);
AU_EXPORT struct passwd* getpwuid(int uid);

#ifdef __cplusplus
}
#endif

#else
#include_next <pwd.h>
#endif
#endif
