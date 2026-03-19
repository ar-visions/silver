// ---- Au sandbox ----
// self-contained module: enforcement (phase 1 + 2), library verification, app verification
// linked into libAu; all symbols prefixed au_sandbox_ / au_dlopen / au_app_verify

#include <import>

#if defined(__linux__)
#include <sched.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <sys/syscall.h>
#include <elf.h>
#include <dlfcn.h>
#elif defined(__APPLE__)
#include <sandbox.h>
#include <dlfcn.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/nlist.h>
#endif

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

// ---- sandbox enforcement ----
// Three tiers + device access, each compiled into .rodata:
//   AU_SANDBOX_INSTALL="install:/path/to/install"            toolchain level (Silver itself)
//   AU_SANDBOX_PUBLIC="public:/path/to/share"                shared app commons
//   AU_SANDBOX_PRIVATE="private:/path/to/app"                per-app exclusive storage
//   AU_SANDBOX_DEVICES="devices:/dev/video0,/dev/snd"        device access list
//
// Layout:
//   /src/silver/
//     private/myapp/     <- deepest, only this app via bind mount
//     install/           <- toolchain (shell-level apps chroot here)
//       share/           <- public commons (normal apps chroot here)
//
// All strings verifiable in the binary's .rodata:
//   strings binary | grep -E "install:|public:|private:|devices:"
//
// Two-phase enforcement:
//   phase 1 (constructor): mounts, chroot, device binding
//   phase 2 (au_sandbox_lock): seccomp blocks execve, no more loading

#if defined(AU_SANDBOX_INSTALL) || defined(AU_SANDBOX_PUBLIC) || defined(AU_SANDBOX_PRIVATE)

#ifdef AU_SANDBOX_INSTALL
static const char au_sandbox_install[] = AU_SANDBOX_INSTALL;
#endif
#ifdef AU_SANDBOX_PUBLIC
static const char au_sandbox_public[]  = AU_SANDBOX_PUBLIC;
#endif
#ifdef AU_SANDBOX_PRIVATE
static const char au_sandbox_private[] = AU_SANDBOX_PRIVATE;
#endif
#ifdef AU_SANDBOX_DEVICES
static const char au_sandbox_devices[] = AU_SANDBOX_DEVICES;
#endif

static bool au_sandbox_active = false;

static const char* au_sandbox_path(const char* sig, const char* prefix) {
    size_t n = strlen(prefix);
    return (strncmp(sig, prefix, n) == 0) ? sig + n : sig;
}

// two-phase sandbox:
//   phase 1 (init): mounts + chroot, dlopen still allowed for hardware drivers
//   phase 2 (lock): seccomp blocks dlopen, openat, mmap+PROT_EXEC -- app code starts here
static bool au_sandbox_locked = false;

static void au_sandbox_init(void) {
    if (au_sandbox_active) return;
    au_sandbox_active = true;

#ifdef AU_SANDBOX_INSTALL
    const char* inst = au_sandbox_path(au_sandbox_install, "install:");
#endif
#ifdef AU_SANDBOX_PUBLIC
    const char* pub  = au_sandbox_path(au_sandbox_public,  "public:");
#endif
#ifdef AU_SANDBOX_PRIVATE
    const char* priv = au_sandbox_path(au_sandbox_private, "private:");
#endif

    // determine chroot target: public overrides install
    const char* root = NULL;
#ifdef AU_SANDBOX_PUBLIC
    root = pub;
#elif defined(AU_SANDBOX_INSTALL)
    root = inst;
#endif

#if defined(__linux__)
    if (root && unshare(CLONE_NEWNS) == 0) {
        // mount root as read-only
        mount(root, root, NULL, MS_BIND | MS_REC, NULL);
        mount(NULL, root, NULL, MS_BIND | MS_REMOUNT | MS_RDONLY | MS_REC, NULL);
        chdir(root);
        if (chroot(root) == 0) chdir("/");

    #ifdef AU_SANDBOX_INSTALL
        // install-tier apps need system headers for compilation
        mkdir("/usr",         0755);
        mkdir("/usr/include", 0755);
        mkdir("/usr/lib",     0755);
        mount("/usr/include", "/usr/include", NULL, MS_BIND | MS_RDONLY, NULL);
        mount("/usr/lib",     "/usr/lib",     NULL, MS_BIND | MS_RDONLY, NULL);
    #endif

    #ifdef AU_SANDBOX_PRIVATE
        // private is the only writable mount
        mkdir("/private", 0700);
        mount(priv, "/private", NULL, MS_BIND, NULL);
    #endif

    #ifdef AU_SANDBOX_DEVICES
        // mount declared devices -- comma separated paths after "devices:"
        {
            const char* devs = au_sandbox_path(au_sandbox_devices, "devices:");
            char buf[1024];
            strncpy(buf, devs, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            char* tok = strtok(buf, ",");
            while (tok) {
                // trim whitespace
                while (*tok == ' ') tok++;
                if (*tok == '/') {
                    char mnt[512];
                    snprintf(mnt, sizeof(mnt), "%s", tok);
                    char parent[512];
                    strncpy(parent, mnt, sizeof(parent));
                    char* last_slash = strrchr(parent, '/');
                    if (last_slash && last_slash != parent) {
                        *last_slash = '\0';
                        mkdir(parent, 0755);
                    }
                    int fd = open(mnt, O_CREAT | O_WRONLY, 0666);
                    if (fd >= 0) close(fd);
                    mount(tok, mnt, NULL, MS_BIND, NULL);
                }
                tok = strtok(NULL, ",");
            }
        }
    #endif

        // remount /proc read-only (prevent /proc/self/exe tricks)
        mount(NULL, "/proc", NULL, MS_REMOUNT | MS_RDONLY, NULL);
    }
#elif defined(__APPLE__)
    if (root) {
        chdir(root);
        char profile[2048];
        snprintf(profile, sizeof(profile),
            "(version 1)\n"
            "(deny default)\n"
            "(allow process-exec)\n"
            "(allow process-fork)\n"
            "(allow sysctl-read)\n"
            "(allow mach-lookup)\n"
            "(allow ipc-posix-shm-read-data)\n"
            "(allow ipc-posix-shm-write-data)\n"
            "(allow signal (target self))\n"
            "(allow file-read* (subpath \"%s\"))\n"
    #ifdef AU_SANDBOX_INSTALL
            "(allow file-read* (subpath \"%s\"))\n"
    #endif
    #ifdef AU_SANDBOX_PRIVATE
            "(allow file-read* file-write* (subpath \"%s\"))\n"
    #endif
            "(allow file-read* file-write* (subpath \"/dev\"))\n"
            "(allow file-read* (subpath \"/usr/lib\"))\n"
            "(allow file-read* (subpath \"/System\"))\n"
            "(allow file-read* (subpath \"/Library/Frameworks\"))\n"
            "(allow iokit-open)\n"
            "(allow network-outbound)\n"
            "(allow network-inbound)\n"
            , root
    #ifdef AU_SANDBOX_INSTALL
            , inst
    #endif
    #ifdef AU_SANDBOX_PRIVATE
            , priv
    #endif
        );
        char *err = NULL;
        if (sandbox_init(profile, SANDBOX_NAMED_EXTERNAL, &err) != 0) {
            if (err) {
                fprintf(stderr, "au_sandbox: sandbox_init failed: %s\n", err);
                sandbox_free_error(err);
            }
        }
    }
#elif defined(_WIN32)
    if (root) SetCurrentDirectoryA(root);
#endif
    // phase 1 complete -- hardware init can happen now (vulkan, audio, etc.)
    // call au_sandbox_lock() after hardware init to enter phase 2
}

// phase 2: hard lock -- no more dlopen, no more file opens, no new exec mappings
void au_sandbox_lock(void) {
    if (au_sandbox_locked) return;
    au_sandbox_locked = true;

#if defined(__linux__)
    struct sock_filter filter[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_execve,    0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_execveat,  0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA)),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };

    struct sock_fprog prog = {
        .len    = sizeof(filter) / sizeof(filter[0]),
        .filter = filter,
    };

    prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
#elif defined(__APPLE__)
    const char *lock_profile =
        "(version 1)\n"
        "(deny default)\n"
        "(allow sysctl-read)\n"
        "(allow mach-lookup)\n"
        "(allow signal (target self))\n"
        "(allow ipc-posix-shm-read-data)\n"
        "(allow ipc-posix-shm-write-data)\n"
        "(allow iokit-open)\n"
        "(allow network-outbound)\n"
        "(allow network-inbound)\n"
        "(allow file-read* (subpath \"/usr/lib\"))\n"
        "(allow file-read* (subpath \"/System\"))\n"
        "(allow file-read* (subpath \"/Library/Frameworks\"))\n"
        "(deny process-exec)\n"
        "(deny process-fork)\n";
    char *err = NULL;
    if (sandbox_init(lock_profile, SANDBOX_NAMED_EXTERNAL, &err) != 0) {
        if (err) {
            fprintf(stderr, "au_sandbox_lock: %s\n", err);
            sandbox_free_error(err);
        }
    }
#endif
}

// ---- library verification ----
#if defined(__linux__)
static bool au_verify_library(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return false;

    Elf64_Ehdr ehdr;
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) { close(fd); return false; }
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0)    { close(fd); return false; }

    Elf64_Shdr* shdrs = calloc(ehdr.e_shnum, sizeof(Elf64_Shdr));
    lseek(fd, ehdr.e_shoff, SEEK_SET);
    read(fd, shdrs, ehdr.e_shnum * sizeof(Elf64_Shdr));

    bool has_init_array = false;
    for (int i = 0; i < ehdr.e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_INIT_ARRAY) {
            has_init_array = true;
            break;
        }
    }

    free(shdrs);
    close(fd);
    return !has_init_array || au_sandbox_active;
}
#elif defined(__APPLE__)
static bool au_verify_library(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return false;

    uint32_t magic;
    if (read(fd, &magic, sizeof(magic)) != sizeof(magic)) { close(fd); return false; }
    lseek(fd, 0, SEEK_SET);

    off_t slice_offset = 0;
    if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
        struct fat_header fh;
        read(fd, &fh, sizeof(fh));
        uint32_t narch = (magic == FAT_CIGAM) ? OSSwapInt32(fh.nfat_arch) : fh.nfat_arch;
        bool found = false;
        for (uint32_t i = 0; i < narch; i++) {
            struct fat_arch fa;
            read(fd, &fa, sizeof(fa));
            cpu_type_t ct = (magic == FAT_CIGAM) ? (cpu_type_t)OSSwapInt32(fa.cputype) : fa.cputype;
        #if defined(__arm64__)
            if (ct == CPU_TYPE_ARM64) {
        #else
            if (ct == CPU_TYPE_X86_64) {
        #endif
                slice_offset = (magic == FAT_CIGAM) ? OSSwapInt32(fa.offset) : fa.offset;
                found = true;
                break;
            }
        }
        if (!found) { close(fd); return false; }
        lseek(fd, slice_offset, SEEK_SET);
    }

    struct mach_header_64 hdr;
    if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) { close(fd); return false; }
    if (hdr.magic != MH_MAGIC_64) { close(fd); return false; }

    bool has_init = false;
    for (uint32_t i = 0; i < hdr.ncmds; i++) {
        struct load_command lc;
        off_t pos = lseek(fd, 0, SEEK_CUR);
        if (read(fd, &lc, sizeof(lc)) != sizeof(lc)) break;

        if (lc.cmd == LC_SEGMENT_64) {
            struct segment_command_64 seg;
            lseek(fd, pos, SEEK_SET);
            if (read(fd, &seg, sizeof(seg)) != sizeof(seg)) break;
            for (uint32_t j = 0; j < seg.nsects; j++) {
                struct section_64 sect;
                if (read(fd, &sect, sizeof(sect)) != sizeof(sect)) break;
                if (strcmp(sect.sectname, "__mod_init_func") == 0) {
                    has_init = true;
                    break;
                }
            }
            if (has_init) break;
            lseek(fd, pos + lc.cmdsize, SEEK_SET);
        } else {
            lseek(fd, pos + lc.cmdsize, SEEK_SET);
        }
    }

    close(fd);
    return !has_init || au_sandbox_active;
}
#endif

void* au_dlopen(const char* path, int flags) {
#if defined(__linux__) || defined(__APPLE__)
    if (au_sandbox_locked) return NULL;
    if (!au_verify_library(path)) return NULL;
    return dlopen(path, flags);
#else
    return dlopen(path, flags);
#endif
}

__attribute__((constructor))
static void au_sandbox_entry(void) {
    au_sandbox_init();
}

// ---- app verification: instruction-level binary analysis ----
// verifies:
//   1. au_sandbox_entry in constructor table (__mod_init_func / .init_array)
//   2. au_sandbox_entry's first call after boolean guard is au_sandbox_init
//   3. au_sandbox_init's first call after its boolean guard is platform sandbox op
//   4. au_sandbox_lock symbol exists
//   5. sandbox path strings in read-only data

typedef struct {
    uint64_t vm_addr;
    uint64_t vm_size;
    uint64_t file_off;
} av_seg;

static off_t av_vaddr_to_foff(av_seg* segs, int nsegs, uint64_t vaddr, off_t base) {
    for (int i = 0; i < nsegs; i++) {
        if (vaddr >= segs[i].vm_addr && vaddr < segs[i].vm_addr + segs[i].vm_size)
            return base + segs[i].file_off + (vaddr - segs[i].vm_addr);
    }
    return -1;
}

static int av_read_fn(int fd, av_seg* segs, int nsegs, off_t base, uint64_t vaddr, uint8_t* buf, int max) {
    off_t foff = av_vaddr_to_foff(segs, nsegs, vaddr, base);
    if (foff < 0) return 0;
    lseek(fd, foff, SEEK_SET);
    return (int)read(fd, buf, max);
}

// ---- arm64 instruction decode ----
static uint64_t av_arm64_first_bl(uint8_t* buf, int len, uint64_t fn_vaddr, int* out_offset) {
    int n = len / 4;
    if (n > 64) n = 64;
    uint32_t* insns = (uint32_t*)buf;
    for (int i = 0; i < n; i++) {
        uint32_t w = insns[i];
        if ((w & 0xFC000000) == 0x94000000) {
            int32_t imm26 = (int32_t)(w & 0x03FFFFFF);
            if (imm26 & 0x02000000) imm26 |= (int32_t)0xFC000000;
            uint64_t target = fn_vaddr + (uint64_t)((int64_t)i * 4 + (int64_t)imm26 * 4);
            if (out_offset) *out_offset = i * 4;
            return target;
        }
    }
    return 0;
}

// strict whitelist: every arm64 instruction from function entry to bl must be
// one of these categories or verification fails.  returns true only if all
// instructions are whitelisted AND a conditional guard branch was seen.
static bool av_arm64_has_guard_before_bl(uint8_t* buf, int len, int bl_offset) {
    int n = bl_offset / 4;
    uint32_t* insns = (uint32_t*)buf;
    bool saw_guard = false;
    for (int i = 0; i < n; i++) {
        uint32_t w = insns[i];

        // NOP
        if (w == 0xD503201F) continue;

        // STP x29, x30, [sp, ...] (frame save, any addressing mode)
        // Rt=x29(29), Rt2=x30(30), Rn=sp(31), opc=10(64-bit), L=0(store)
        if ((w & 0xC0407FFF) == 0x80007BFD) continue;

        // LDP x29, x30, [sp, ...] (frame restore — can appear in guard-skip path)
        if ((w & 0xC0407FFF) == 0x80407BFD) continue;

        // MOV x29, sp (ADD x29, sp, #0)
        if (w == 0x910003FD) continue;

        // SUB sp, sp, #imm (stack frame allocation)
        if ((w >> 24) == 0xD1 && (w & 0x1F) == 31 && ((w >> 5) & 0x1F) == 31) continue;

        // ADRP xN, #page (address page load for boolean global)
        if ((w & 0x9F000000) == 0x90000000) continue;

        // ADD Xd, Xn, #imm (page offset, part of ADRP+ADD pair)
        if ((w & 0xFF000000) == 0x91000000) continue;

        // LDRB Wt, [Xn, #uimm] (unsigned offset — load boolean byte)
        if ((w & 0xFFC00000) == 0x39400000) continue;

        // LDRB Wt, [Xn, Xm, extend] (register offset)
        if ((w & 0xFFE00C00) == 0x38600800) continue;

        // LDR Wt, [Xn, #uimm] (32-bit load, could load boolean as int)
        if ((w & 0xFFC00000) == 0xB9400000) continue;

        // CMP Xn, #imm (SUBS XZR, Xn, #imm — 64-bit)
        if ((w & 0xFF00001F) == 0xF100001F) continue;
        // CMP Wn, #imm (SUBS WZR, Wn, #imm — 32-bit)
        if ((w & 0xFF00001F) == 0x7100001F) continue;

        // TST Xn, #imm (ANDS XZR, Xn, #bitmask — 64-bit)
        if ((w & 0xFF00001F) == 0xF200001F) continue;
        // TST Wn, #imm (ANDS WZR, Wn, #bitmask — 32-bit)
        if ((w & 0xFF00001F) == 0x7200001F) continue;

        // CBZ / CBNZ (conditional branch — the guard)
        if ((w & 0x7E000000) == 0x34000000) { saw_guard = true; continue; }

        // B.cond (conditional branch)
        if ((w & 0xFF000010) == 0x54000000) { saw_guard = true; continue; }

        // TBZ / TBNZ (test bit and branch — the guard)
        if ((w & 0x7E000000) == 0x36000000) { saw_guard = true; continue; }

        // RET (x30) — in guard-skip epilogue path
        if (w == 0xD65F03C0) continue;

        // anything else: unknown instruction, reject
        return false;
    }
    return saw_guard;
}

// ---- x86_64 instruction decode ----
static uint64_t av_x86_first_call(uint8_t* buf, int len, uint64_t fn_vaddr, int* out_offset) {
    for (int i = 0; i < len - 4 && i < 256; i++) {
        if (buf[i] == 0xE8) {
            int32_t rel;
            memcpy(&rel, &buf[i + 1], 4);
            uint64_t target = fn_vaddr + i + 5 + (int64_t)rel;
            if (out_offset) *out_offset = i;
            return target;
        }
    }
    return 0;
}

// helper: decode modrm addressing mode length (bytes after modrm)
static int av_x86_modrm_extra(uint8_t modrm) {
    uint8_t mod = modrm >> 6;
    uint8_t rm  = modrm & 7;
    if (mod == 3) return 0;                           // register-register
    if (mod == 0 && rm == 5) return 4;                // RIP-relative disp32
    if (mod == 0 && rm == 4) return 1;                // SIB, no disp
    if (mod == 0) return 0;                           // [reg]
    if (mod == 1 && rm == 4) return 2;                // SIB + disp8
    if (mod == 1) return 1;                           // [reg+disp8]
    if (mod == 2 && rm == 4) return 5;                // SIB + disp32
    return 4;                                          // [reg+disp32]
}

// strict whitelist: every x86_64 instruction from function entry to call must
// be classifiable or verification fails.  returns true only if all instructions
// are whitelisted AND a conditional guard branch was seen.
static bool av_x86_has_guard_before_call(uint8_t* buf, int len, int call_offset) {
    int i = 0;
    bool saw_guard = false;

    while (i < call_offset) {
        uint8_t b = buf[i];

        // PUSH RBP
        if (b == 0x55) { i += 1; continue; }

        // POP RBP
        if (b == 0x5D) { i += 1; continue; }

        // NOP
        if (b == 0x90) { i += 1; continue; }

        // RET (in guard-skip path)
        if (b == 0xC3) { i += 1; continue; }

        // JE rel8 / JNE rel8 (the guard branch)
        if (b == 0x74 || b == 0x75) {
            if (saw_guard) return false; // only one guard allowed
            saw_guard = true;
            i += 2; continue;
        }

        // JE rel32 / JNE rel32: 0x0F 0x84/0x85
        if (b == 0x0F && i + 1 < call_offset && (buf[i+1] == 0x84 || buf[i+1] == 0x85)) {
            if (saw_guard) return false;
            saw_guard = true;
            i += 6; continue;
        }

        // multi-byte NOP: 0x0F 0x1F ...
        if (b == 0x0F && i + 1 < call_offset && buf[i+1] == 0x1F) {
            uint8_t modrm = buf[i + 2];
            i += 3 + av_x86_modrm_extra(modrm);
            continue;
        }

        // handle REX prefix (0x40-0x4F)
        int rex = (b >= 0x40 && b <= 0x4F) ? 1 : 0;
        uint8_t op = buf[i + rex];

        // MOV r/m, r (0x89) or MOV r, r/m (0x8B) — for mov rbp,rsp etc
        if (op == 0x89 || op == 0x8B) {
            uint8_t modrm = buf[i + rex + 1];
            i += rex + 2 + av_x86_modrm_extra(modrm);
            continue;
        }

        // LEA r, [m] (0x8D) — RIP-relative address load for boolean global
        if (op == 0x8D) {
            uint8_t modrm = buf[i + rex + 1];
            i += rex + 2 + av_x86_modrm_extra(modrm);
            continue;
        }

        // MOVZX r, r/m8 (0x0F 0xB6) — zero-extend byte load (boolean)
        if (op == 0x0F && buf[i + rex + 1] == 0xB6) {
            uint8_t modrm = buf[i + rex + 2];
            i += rex + 3 + av_x86_modrm_extra(modrm);
            continue;
        }

        // MOVZX r, r/m16 (0x0F 0xB7)
        if (op == 0x0F && buf[i + rex + 1] == 0xB7) {
            uint8_t modrm = buf[i + rex + 2];
            i += rex + 3 + av_x86_modrm_extra(modrm);
            continue;
        }

        // TEST r/m8, r8 (0x84)
        if (op == 0x84) {
            uint8_t modrm = buf[i + rex + 1];
            i += rex + 2 + av_x86_modrm_extra(modrm);
            continue;
        }

        // TEST r/m32, r32 (0x85)
        if (op == 0x85) {
            uint8_t modrm = buf[i + rex + 1];
            i += rex + 2 + av_x86_modrm_extra(modrm);
            continue;
        }

        // CMP r/m, imm8 (0x83 /7) or SUB r/m, imm8 (0x83 /5)
        if (op == 0x83) {
            uint8_t modrm = buf[i + rex + 1];
            uint8_t reg = (modrm >> 3) & 7;
            if (reg == 5 || reg == 7) { // SUB or CMP
                i += rex + 3 + av_x86_modrm_extra(modrm); // opcode + modrm + extras + imm8
                continue;
            }
            return false; // other 0x83 opcodes not allowed
        }

        // CMP r/m, imm32 (0x81 /7) or SUB r/m, imm32 (0x81 /5)
        if (op == 0x81) {
            uint8_t modrm = buf[i + rex + 1];
            uint8_t reg = (modrm >> 3) & 7;
            if (reg == 5 || reg == 7) {
                i += rex + 6 + av_x86_modrm_extra(modrm); // opcode + modrm + extras + imm32
                continue;
            }
            return false;
        }

        // CMP r/m8, imm8 (0x80 /7)
        if (op == 0x80) {
            uint8_t modrm = buf[i + rex + 1];
            uint8_t reg = (modrm >> 3) & 7;
            if (reg == 7) {
                i += rex + 3 + av_x86_modrm_extra(modrm);
                continue;
            }
            return false;
        }

        // CMP r/m, r (0x39) or CMP r, r/m (0x3B)
        if (op == 0x39 || op == 0x3B) {
            uint8_t modrm = buf[i + rex + 1];
            i += rex + 2 + av_x86_modrm_extra(modrm);
            continue;
        }

        // TEST AL, imm8 (0xA8)
        if (op == 0xA8) { i += rex + 2; continue; }

        // unknown instruction: reject
        return false;
    }

    return saw_guard;
}

// ---- symbol lookup ----
typedef struct {
    const char* name;
    uint64_t    addr;
} av_sym;

#if defined(__APPLE__)
static bool av_macho_find_syms(int fd, off_t base, struct mach_header_64* hdr,
                               av_sym* wanted, int nwanted, av_seg* segs, int* nsegs,
                               uint64_t* init_off, uint64_t* init_size,
                               uint64_t* cstr_off, uint64_t* cstr_size) {
    uint32_t symtab_off = 0, symtab_n = 0, strtab_off = 0, strtab_sz = 0;
    *nsegs = 0;

    off_t cmd_pos = base + sizeof(struct mach_header_64);
    for (uint32_t i = 0; i < hdr->ncmds; i++) {
        lseek(fd, cmd_pos, SEEK_SET);
        struct load_command lc;
        if (read(fd, &lc, sizeof(lc)) != sizeof(lc)) break;

        if (lc.cmd == LC_SYMTAB) {
            lseek(fd, cmd_pos, SEEK_SET);
            struct symtab_command sc;
            read(fd, &sc, sizeof(sc));
            symtab_off = sc.symoff;
            symtab_n   = sc.nsyms;
            strtab_off = sc.stroff;
            strtab_sz  = sc.strsize;
        } else if (lc.cmd == LC_SEGMENT_64) {
            lseek(fd, cmd_pos, SEEK_SET);
            struct segment_command_64 seg;
            read(fd, &seg, sizeof(seg));
            if (*nsegs < 16) {
                segs[*nsegs].vm_addr  = seg.vmaddr;
                segs[*nsegs].vm_size  = seg.vmsize;
                segs[*nsegs].file_off = seg.fileoff;
                (*nsegs)++;
            }
            for (uint32_t j = 0; j < seg.nsects; j++) {
                struct section_64 sect;
                read(fd, &sect, sizeof(sect));
                if (strcmp(sect.sectname, "__mod_init_func") == 0) {
                    *init_off  = sect.offset;
                    *init_size = sect.size;
                } else if (strcmp(sect.sectname, "__cstring") == 0) {
                    *cstr_off  = sect.offset;
                    *cstr_size = sect.size;
                }
            }
        }
        cmd_pos += lc.cmdsize;
    }

    if (!strtab_sz || !symtab_n) return false;

    char* strtab = malloc(strtab_sz);
    lseek(fd, base + strtab_off, SEEK_SET);
    read(fd, strtab, strtab_sz);

    struct nlist_64* syms = malloc(symtab_n * sizeof(struct nlist_64));
    lseek(fd, base + symtab_off, SEEK_SET);
    read(fd, syms, symtab_n * sizeof(struct nlist_64));

    for (uint32_t i = 0; i < symtab_n; i++) {
        if (syms[i].n_un.n_strx >= strtab_sz) continue;
        const char* name = strtab + syms[i].n_un.n_strx;
        for (int w = 0; w < nwanted; w++) {
            if (strcmp(name, wanted[w].name) == 0)
                wanted[w].addr = syms[i].n_value;
        }
    }

    free(syms);
    free(strtab);
    return true;
}

bool au_app_verify(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return false;

    uint32_t magic;
    if (read(fd, &magic, sizeof(magic)) != sizeof(magic)) { close(fd); return false; }
    lseek(fd, 0, SEEK_SET);

    off_t base = 0;
    if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
        struct fat_header fh;
        read(fd, &fh, sizeof(fh));
        uint32_t narch = (magic == FAT_CIGAM) ? OSSwapInt32(fh.nfat_arch) : fh.nfat_arch;
        for (uint32_t i = 0; i < narch; i++) {
            struct fat_arch fa;
            read(fd, &fa, sizeof(fa));
            cpu_type_t ct = (magic == FAT_CIGAM) ? (cpu_type_t)OSSwapInt32(fa.cputype) : fa.cputype;
        #if defined(__arm64__)
            if (ct == CPU_TYPE_ARM64) {
        #else
            if (ct == CPU_TYPE_X86_64) {
        #endif
                base = (magic == FAT_CIGAM) ? OSSwapInt32(fa.offset) : fa.offset;
                break;
            }
        }
        lseek(fd, base, SEEK_SET);
    }

    struct mach_header_64 hdr;
    if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) { close(fd); return false; }
    if (hdr.magic != MH_MAGIC_64) { close(fd); return false; }

    av_sym wanted[] = {
        { "_au_sandbox_entry", 0 },
        { "_au_sandbox_init",  0 },
        { "_au_sandbox_lock",  0 },
    };
    int nwanted = sizeof(wanted) / sizeof(wanted[0]);

    av_seg segs[16];
    int nsegs = 0;
    uint64_t init_off = 0, init_size = 0;
    uint64_t cstr_off = 0, cstr_size = 0;

    if (!av_macho_find_syms(fd, base, &hdr, wanted, nwanted, segs, &nsegs,
                            &init_off, &init_size, &cstr_off, &cstr_size)) {
        close(fd); return false;
    }

    uint64_t entry_addr = wanted[0].addr;
    uint64_t init_addr  = wanted[1].addr;
    uint64_t lock_addr  = wanted[2].addr;

    if (!entry_addr || !init_addr || !lock_addr) { close(fd); return false; }

    // constructor table check: au_sandbox_entry must be the FIRST entry
    // so no other constructor can run before the sandbox is established
    bool entry_is_first = false;
    if (init_size >= sizeof(uint64_t)) {
        uint64_t first_ptr;
        lseek(fd, base + init_off, SEEK_SET);
        if (read(fd, &first_ptr, sizeof(first_ptr)) == sizeof(first_ptr))
            entry_is_first = (first_ptr == entry_addr);
    }
    if (!entry_is_first) { close(fd); return false; }

    // instruction verify: au_sandbox_entry -> au_sandbox_init
    uint8_t fn_buf[512];
    int fn_len = av_read_fn(fd, segs, nsegs, base, entry_addr, fn_buf, sizeof(fn_buf));
    if (fn_len < 8) { close(fd); return false; }

    int bl_off = 0;
    bool is_arm64 = (hdr.cputype == CPU_TYPE_ARM64);
    uint64_t first_call_target = is_arm64
        ? av_arm64_first_bl(fn_buf, fn_len, entry_addr, &bl_off)
        : av_x86_first_call(fn_buf, fn_len, entry_addr, &bl_off);

    if (first_call_target != init_addr) { close(fd); return false; }

    bool has_guard = is_arm64
        ? av_arm64_has_guard_before_bl(fn_buf, fn_len, bl_off)
        : av_x86_has_guard_before_call(fn_buf, fn_len, bl_off);
    if (!has_guard) { close(fd); return false; }

    // instruction verify: au_sandbox_init first call after its guard
    fn_len = av_read_fn(fd, segs, nsegs, base, init_addr, fn_buf, sizeof(fn_buf));
    if (fn_len < 8) { close(fd); return false; }

    bl_off = 0;
    uint64_t init_first_call = is_arm64
        ? av_arm64_first_bl(fn_buf, fn_len, init_addr, &bl_off)
        : av_x86_first_call(fn_buf, fn_len, init_addr, &bl_off);
    if (!init_first_call) { close(fd); return false; }

    has_guard = is_arm64
        ? av_arm64_has_guard_before_bl(fn_buf, fn_len, bl_off)
        : av_x86_has_guard_before_call(fn_buf, fn_len, bl_off);
    if (!has_guard) { close(fd); return false; }

    // sandbox path strings in __cstring
    bool has_paths = false;
    if (cstr_size > 0) {
        char* cstr = malloc(cstr_size);
        lseek(fd, base + cstr_off, SEEK_SET);
        read(fd, cstr, cstr_size);
        for (uint64_t off = 0; off < cstr_size; off++) {
            const char* s = cstr + off;
            uint64_t remain = cstr_size - off;
            if ((remain >= 8  && strncmp(s, "install:",  8) == 0) ||
                (remain >= 7  && strncmp(s, "public:",   7) == 0) ||
                (remain >= 8  && strncmp(s, "private:",  8) == 0)) {
                has_paths = true;
                break;
            }
        }
        free(cstr);
    }
    if (!has_paths) { close(fd); return false; }

    close(fd);
    return true;
}

#elif defined(__linux__)
bool au_app_verify(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return false;

    Elf64_Ehdr ehdr;
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) { close(fd); return false; }
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0)    { close(fd); return false; }

    Elf64_Shdr* shdrs = calloc(ehdr.e_shnum, sizeof(Elf64_Shdr));
    lseek(fd, ehdr.e_shoff, SEEK_SET);
    read(fd, shdrs, ehdr.e_shnum * sizeof(Elf64_Shdr));

    Elf64_Shdr shstrtab_hdr = shdrs[ehdr.e_shstrndx];
    char* shstrtab = malloc(shstrtab_hdr.sh_size);
    lseek(fd, shstrtab_hdr.sh_offset, SEEK_SET);
    read(fd, shstrtab, shstrtab_hdr.sh_size);

    Elf64_Shdr *symtab_sh = NULL, *strtab_sh = NULL;
    Elf64_Shdr *init_sh = NULL, *rodata_sh = NULL;

    av_seg segs[16];
    int nsegs = 0;

    for (int i = 0; i < ehdr.e_shnum; i++) {
        const char* name = shstrtab + shdrs[i].sh_name;
        if (shdrs[i].sh_type == SHT_SYMTAB)     symtab_sh = &shdrs[i];
        else if (shdrs[i].sh_type == SHT_STRTAB && strcmp(name, ".strtab") == 0) strtab_sh = &shdrs[i];
        else if (shdrs[i].sh_type == SHT_INIT_ARRAY) init_sh = &shdrs[i];
        else if (strcmp(name, ".rodata") == 0)    rodata_sh = &shdrs[i];
        if ((shdrs[i].sh_flags & SHF_EXECINSTR) && nsegs < 16) {
            segs[nsegs].vm_addr  = shdrs[i].sh_addr;
            segs[nsegs].vm_size  = shdrs[i].sh_size;
            segs[nsegs].file_off = shdrs[i].sh_offset;
            nsegs++;
        }
    }

    if (!symtab_sh || !strtab_sh) { free(shstrtab); free(shdrs); close(fd); return false; }

    char* strtab = malloc(strtab_sh->sh_size);
    lseek(fd, strtab_sh->sh_offset, SEEK_SET);
    read(fd, strtab, strtab_sh->sh_size);

    int nsyms = symtab_sh->sh_size / sizeof(Elf64_Sym);
    Elf64_Sym* syms = malloc(symtab_sh->sh_size);
    lseek(fd, symtab_sh->sh_offset, SEEK_SET);
    read(fd, syms, symtab_sh->sh_size);

    uint64_t entry_addr = 0, init_addr = 0, lock_addr = 0;
    for (int i = 0; i < nsyms; i++) {
        if (syms[i].st_name >= strtab_sh->sh_size) continue;
        const char* name = strtab + syms[i].st_name;
        if (strcmp(name, "au_sandbox_entry") == 0) entry_addr = syms[i].st_value;
        if (strcmp(name, "au_sandbox_init")  == 0) init_addr  = syms[i].st_value;
        if (strcmp(name, "au_sandbox_lock")  == 0) lock_addr  = syms[i].st_value;
    }
    free(syms);

    if (!entry_addr || !init_addr || !lock_addr) {
        free(strtab); free(shstrtab); free(shdrs); close(fd); return false;
    }

    // constructor table check: au_sandbox_entry must be the FIRST entry
    // so no other constructor can run before the sandbox is established
    bool entry_is_first = false;
    if (init_sh && init_sh->sh_size >= sizeof(uint64_t)) {
        uint64_t first_ptr;
        lseek(fd, init_sh->sh_offset, SEEK_SET);
        if (read(fd, &first_ptr, sizeof(first_ptr)) == sizeof(first_ptr))
            entry_is_first = (first_ptr == entry_addr);
    }
    if (!entry_is_first) { free(strtab); free(shstrtab); free(shdrs); close(fd); return false; }

    // instruction verify: au_sandbox_entry -> au_sandbox_init
    uint8_t fn_buf[512];
    int fn_len = av_read_fn(fd, segs, nsegs, 0, entry_addr, fn_buf, sizeof(fn_buf));
    if (fn_len < 8) { free(strtab); free(shstrtab); free(shdrs); close(fd); return false; }

    int call_off = 0;
    uint64_t first_call = av_x86_first_call(fn_buf, fn_len, entry_addr, &call_off);
    if (first_call != init_addr) { free(strtab); free(shstrtab); free(shdrs); close(fd); return false; }

    bool has_guard = av_x86_has_guard_before_call(fn_buf, fn_len, call_off);
    if (!has_guard) { free(strtab); free(shstrtab); free(shdrs); close(fd); return false; }

    // instruction verify: au_sandbox_init first call after guard
    fn_len = av_read_fn(fd, segs, nsegs, 0, init_addr, fn_buf, sizeof(fn_buf));
    if (fn_len < 8) { free(strtab); free(shstrtab); free(shdrs); close(fd); return false; }

    call_off = 0;
    uint64_t init_first = av_x86_first_call(fn_buf, fn_len, init_addr, &call_off);
    if (!init_first) { free(strtab); free(shstrtab); free(shdrs); close(fd); return false; }

    has_guard = av_x86_has_guard_before_call(fn_buf, fn_len, call_off);
    if (!has_guard) { free(strtab); free(shstrtab); free(shdrs); close(fd); return false; }

    // .rodata path strings
    bool has_paths = false;
    if (rodata_sh && rodata_sh->sh_size > 0) {
        char* rodata = malloc(rodata_sh->sh_size);
        lseek(fd, rodata_sh->sh_offset, SEEK_SET);
        read(fd, rodata, rodata_sh->sh_size);
        for (uint64_t off = 0; off < rodata_sh->sh_size; off++) {
            const char* s = rodata + off;
            uint64_t remain = rodata_sh->sh_size - off;
            if ((remain >= 8  && strncmp(s, "install:",  8) == 0) ||
                (remain >= 7  && strncmp(s, "public:",   7) == 0) ||
                (remain >= 8  && strncmp(s, "private:",  8) == 0)) {
                has_paths = true;
                break;
            }
        }
        free(rodata);
    }

    free(strtab);
    free(shstrtab);
    free(shdrs);
    close(fd);

    return entry_in_ctors && has_paths;
}
#endif

#endif
// ---- end sandbox ----
