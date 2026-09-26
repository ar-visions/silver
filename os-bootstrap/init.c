// orbiter-os /init: mount the drive, enter its folder, run
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/syscall.h>

extern char** environ;
static char cmdline[4096];

// pid 1 must never return: say why and wait
static void die(const char* why, const char* what) {
    fprintf(stderr, "orbiter-os init: %s %s\n", why, what ? what : "");
    for (;;) pause();
}

// a key=value word's value on the kernel command line
static const char* arg(const char* key, char* out, size_t cap) {
    size_t n = strlen(key);
    const char* p = cmdline;
    while (*p) {
        while (*p == ' ' || *p == '\n') p++;
        const char* e = p;
        while (*e && *e != ' ' && *e != '\n') e++;
        if ((size_t)(e - p) > n && !strncmp(p, key, n) && p[n] == '=') {
            size_t m = (size_t)(e - p) - n - 1;
            if (m >= cap) m = cap - 1;
            memcpy(out, p + n + 1, m);
            out[m] = 0;
            return out;
        }
        p = e;
    }
    return NULL;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// 5069068c-dbe7-... to its 16 bytes
static int parse_uuid(const char* s, unsigned char u[16]) {
    int k = 0;
    for (; *s && k < 32; s++) {
        if (*s == '-') continue;
        int v = hexval(*s);
        if (v < 0) return 0;
        if (k & 1) u[k / 2] |= (unsigned char)v;
        else       u[k / 2]  = (unsigned char)(v << 4);
        k++;
    }
    return k == 32;
}

// the block device whose ext4 superblock has this uuid
static int find_uuid(const unsigned char want[16], char* dev, size_t cap) {
    DIR* d = opendir("/sys/class/block");
    if (!d) return 0;
    struct dirent* e;
    int found = 0;
    while (!found && (e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        snprintf(dev, cap, "/dev/%s", e->d_name);
        int fd = open(dev, O_RDONLY);
        if (fd < 0) continue;
        // superblock at 1024: magic at +56, uuid at +104
        unsigned char sb[1024];
        found = pread(fd, sb, sizeof sb, 1024) == (ssize_t)sizeof sb &&
                sb[56] == 0x53 && sb[57] == 0xEF && !memcmp(sb + 104, want, 16);
        close(fd);
    }
    closedir(d);
    return found;
}

// the kernel log and pid 1's output, on the drive
static void save_logs(void) {
    int out = open("/boot/boot.log", O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0644);
    if (out >= 0) { dup2(out, 1); dup2(out, 2); close(out); }
    if (fork() != 0) return;
    // the child: every kernel line, synced, until power off
    int km = open("/dev/kmsg", O_RDONLY);
    int kf = open("/boot/kmsg.log", O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0644);
    if (km < 0 || kf < 0) _exit(1);
    char line[8192];
    for (;;) {
        ssize_t n = read(km, line, sizeof line);
        if (n > 0) { if (write(kf, line, (size_t)n) < 0) _exit(1); }
        else if (n < 0) usleep(1000);
    }
}

// orbiter.modules=a,b:k=v loads /lib/modules/orbiter/a.ko ...
static void load_modules(void) {
    char list[1024];
    if (!arg("orbiter.modules", list, sizeof list)) return;
    for (char* name = strtok(list, ","); name; name = strtok(NULL, ",")) {
        char* params = strchr(name, ':');
        if (params) *params++ = 0;
        char ko[512];
        snprintf(ko, sizeof ko, "/lib/modules/orbiter/%s.ko", name);
        int fd = open(ko, O_RDONLY | O_CLOEXEC);
        if (fd < 0 || syscall(SYS_finit_module, fd, params ? params : "", 0))
            fprintf(stderr, "orbiter-os init: module %s not loaded\n", name);
        if (fd >= 0) close(fd);
    }
    // nvidia has no udev here: its nodes, major 195
    if (access("/proc/driver/nvidia", F_OK) == 0) {
        mknod("/dev/nvidiactl", S_IFCHR | 0666, makedev(195, 255));
        mknod("/dev/nvidia-modeset", S_IFCHR | 0666, makedev(195, 254));
        DIR* g = opendir("/proc/driver/nvidia/gpus");
        struct dirent* e;
        int n = 0;
        while (g && (e = readdir(g)))
            if (e->d_name[0] != '.') {
                char node[32];
                snprintf(node, sizeof node, "/dev/nvidia%d", n);
                mknod(node, S_IFCHR | 0666, makedev(195, n++));
            }
        if (g) closedir(g);
        // TEMP trace: can each nvidia node be opened
        const char* nodes[] = { "/dev/nvidiactl", "/dev/nvidia0", "/dev/nvidia-modeset" };
        for (int i = 0; i < 3; i++) {
            int t = open(nodes[i], O_RDWR);
            fprintf(stderr, "init: open %s -> %s\n", nodes[i], t >= 0 ? "ok" : strerror(errno));
            if (t >= 0) close(t);
        }
        fprintf(stderr, "init: nvidia gpus %d\n", n);
    }
}

int main(void) {
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    int con = open("/dev/console", O_RDWR);
    if (con >= 0) { dup2(con, 0); dup2(con, 1); dup2(con, 2); }

    int cf = open("/proc/cmdline", O_RDONLY);
    if (cf >= 0) {
        ssize_t n = read(cf, cmdline, sizeof cmdline - 1);
        cmdline[n > 0 ? n : 0] = 0;
        close(cf);
    }
    char uuid_s[64], dir[256], init[512];
    if (!arg("orbiter.uuid", uuid_s, sizeof uuid_s)) die("no orbiter.uuid", NULL);
    if (!arg("orbiter.root", dir, sizeof dir)) strcpy(dir, "/orbiter-os");
    if (!arg("orbiter.init", init, sizeof init)) die("no orbiter.init", NULL);
    unsigned char uuid[16];
    if (!parse_uuid(uuid_s, uuid)) die("bad uuid", uuid_s);

    // the nvme drive appears a moment after boot: 10 s
    char dev[128];
    int found = 0;
    for (int i = 0; i < 100 && !found; i++) {
        found = find_uuid(uuid, dev, sizeof dev);
        if (!found) usleep(100000);
    }
    if (!found) die("no partition with uuid", uuid_s);

    mkdir("/mnt", 0755);
    if (mount(dev, "/mnt", "ext4", 0, NULL)) die("cannot mount", dev);
    // the relative /orbiter-os link resolves inside /mnt
    char full[1024], real[4096];
    snprintf(full, sizeof full, "/mnt%s", dir);
    if (!realpath(full, real)) die("no folder", full);

    mkdir("/newroot", 0755);
    if (mount(real, "/newroot", NULL, MS_BIND, NULL)) die("cannot bind", real);
    mount("/dev", "/newroot/dev", NULL, MS_MOVE, NULL);
    mount("/proc", "/newroot/proc", NULL, MS_MOVE, NULL);
    mount("/sys", "/newroot/sys", NULL, MS_MOVE, NULL);
    // scratch stays in memory, not in the project tree
    mount("tmpfs", "/newroot/tmp", "tmpfs", 0, NULL);
    mount("tmpfs", "/newroot/run", "tmpfs", 0, NULL);

    // the switch_root move: rootfs cannot be pivoted
    if (chdir("/newroot")) die("cannot enter", real);
    if (mount(".", "/", NULL, MS_MOVE, NULL)) die("cannot move root", real);
    if (chroot(".") || chdir("/")) die("cannot chroot", real);
    save_logs();
    // after the switch: firmware loads from the folder
    load_modules();

    char* argv[] = { init, NULL };
    execve(init, argv, environ);
    die("cannot run", init);
    return 1;
}
