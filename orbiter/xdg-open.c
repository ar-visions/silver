// orbiter's xdg-open: an address opened by a program orbiter started
// (ORBITER_SOCKET in its environment) goes to orbiter, which shows it
// in aura; any other program's goes to the system's xdg-open
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/un.h>

// the system's xdg-open, never this one (orbiter-os has only this)
static int system_open(char** argv) {
    char self[PATH_MAX], other[PATH_MAX];
    const char* sys = "/usr/bin/xdg-open";
    if (realpath("/proc/self/exe", self) && realpath(sys, other) &&
        strcmp(self, other) != 0) {
        execv(sys, argv);
        perror("xdg-open");
    }
    fprintf(stderr, "xdg-open: no system xdg-open and no orbiter\n");
    return 3;
}

int main(int argc, char** argv) {
    if (argc < 2 || !*argv[1]) {
        fprintf(stderr, "usage: xdg-open <url>\n");
        return 1;
    }
    const char* sock = getenv("ORBITER_SOCKET");
    if (!sock || !*sock) return system_open(argv);
    struct sockaddr_un su;
    memset(&su, 0, sizeof(su));
    su.sun_family = AF_UNIX;
    snprintf(su.sun_path, sizeof(su.sun_path), "%s", sock);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0 || connect(fd, (struct sockaddr*)&su, sizeof(su)) != 0) {
        if (fd >= 0) close(fd);
        return system_open(argv);
    }
    char line[8192];
    int n = snprintf(line, sizeof(line), "app open %s\n", argv[1]);
    if (n <= 0 || n >= (int)sizeof(line) || write(fd, line, n) != n) {
        close(fd);
        return 4;
    }
    // orbiter answers once the line is taken
    char reply[64];
    ssize_t r = read(fd, reply, sizeof(reply) - 1);
    close(fd);
    if (r <= 0) return 4;
    reply[r] = 0;
    return strncmp(reply, "ok", 2) == 0 ? 0 : 4;
}
