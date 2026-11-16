#ifdef _WIN32
#define _CRT_RAND_S
#else
// glibc declares fseeko only past -std=c11's strict set
#define _DEFAULT_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <zlib.h>
#include "disk_gpt.h"
#ifdef _WIN32
#define fseeko _fseeki64
#define off_t long long
#endif

// GPT by the UEFI spec: no sfdisk, so any system can write it

static void le(uint8_t* p, uint64_t v, int n) {
    for (int i = 0; i < n; i++) p[i] = (uint8_t)(v >> (8 * i));
}

// a random GUID (version 4), as GPT stores it
static int guid(uint8_t* g) {
#ifdef _WIN32
    size_t n = 16;
    for (int i = 0; i < 16; i += 4) {
        unsigned int r;
        if (rand_s(&r)) return 0;
        memcpy(g + i, &r, 4);
    }
#else
    FILE* f = fopen("/dev/urandom", "rb");
    if (!f) return 0;
    size_t n = fread(g, 1, 16, f);
    fclose(f);
#endif
    g[7] = (g[7] & 0x0f) | 0x40;
    g[8] = (g[8] & 0x3f) | 0x80;
    return n == 16;
}

static void name16(uint8_t* p, const char* s) {
    for (int i = 0; s[i] && i < 36; i++) p[2 * i] = (uint8_t)s[i];
}

static void header(uint8_t* h, uint64_t self, uint64_t other,
                   uint64_t entries, uint64_t last, const uint8_t* dg,
                   uint32_t ecrc) {
    memset(h, 0, 512);
    memcpy(h, "EFI PART", 8);
    le(h + 8, 0x00010000, 4);
    le(h + 12, 92, 4);
    le(h + 24, self, 8);
    le(h + 32, other, 8);
    le(h + 40, 34, 8);
    le(h + 48, last, 8);
    memcpy(h + 56, dg, 16);
    le(h + 72, entries, 8);
    le(h + 80, 128, 4);
    le(h + 84, 128, 4);
    le(h + 88, ecrc, 4);
    le(h + 16, crc32(0, h, 92), 4);
}

static int put(FILE* f, uint64_t lba, const uint8_t* b, size_t n) {
    if (fseeko(f, (off_t)(lba * 512), SEEK_SET)) return 0;
    return fwrite(b, 1, n, f) == n;
}

int disk_gpt(const char* img, long long sectors, long long efi_sectors) {
    static const uint8_t efi_t[16] = { 0x28, 0x73, 0x2a, 0xc1, 0x1f,
        0xf8, 0xd2, 0x11, 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b };
    static const uint8_t linux_t[16] = { 0xaf, 0x3d, 0xc6, 0x0f, 0x83,
        0x84, 0x72, 0x47, 0x8e, 0x79, 0x3d, 0x69, 0xd8, 0x47, 0x7d, 0xe4 };
    uint64_t total = (uint64_t)sectors, last = total - 34;
    uint64_t root0 = 2048 + (uint64_t)efi_sectors;
    if (total < root0 + 2048 + 34) return 0;
    uint8_t mbr[512] = { 0 }, ent[16384] = { 0 }, h[512], dg[16];
    // the protective MBR: one 0xEE entry over the disk
    mbr[446 + 2] = 0x02;
    mbr[446 + 4] = 0xee;
    memset(mbr + 446 + 5, 0xff, 3);
    le(mbr + 446 + 8, 1, 4);
    le(mbr + 446 + 12, total - 1 > 0xffffffffu ? 0xffffffffu
        : total - 1, 4);
    mbr[510] = 0x55;
    mbr[511] = 0xaa;
    if (!guid(dg) || !guid(ent + 16) || !guid(ent + 128 + 16)) return 0;
    memcpy(ent, efi_t, 16);
    le(ent + 32, 2048, 8);
    le(ent + 40, root0 - 1, 8);
    name16(ent + 56, "orbiter-efi");
    memcpy(ent + 128, linux_t, 16);
    le(ent + 128 + 32, root0, 8);
    le(ent + 128 + 40, last, 8);
    name16(ent + 128 + 56, "orbiter");
    uint32_t ec = (uint32_t)crc32(0, ent, sizeof ent);
    FILE* f = fopen(img, "r+b");
    if (!f) return 0;
    int ok = put(f, 0, mbr, 512) && put(f, 2, ent, sizeof ent);
    header(h, 1, total - 1, 2, last, dg, ec);
    ok = ok && put(f, 1, h, 512);
    ok = ok && put(f, total - 33, ent, sizeof ent);
    header(h, total - 1, 1, total - 33, last, dg, ec);
    ok = ok && put(f, total - 1, h, 512);
    return fclose(f) == 0 && ok;
}
