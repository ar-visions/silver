// protobuf, as much as Google Play's messages need: a writer that
// nests messages, and a reader that follows a field path in a file
#define _DEFAULT_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <elf.h>

// ---- writer -----------------------------------------------------------

typedef struct {
    uint8_t* data;
    int64_t  len, cap;
    int64_t  open[16];      // where each open message's body starts
    int      depth;
} pbw;

static void pbw_room(pbw* w, int64_t n) {
    if (w->len + n <= w->cap) return;
    while (w->len + n > w->cap) w->cap = w->cap ? w->cap * 2 : 256;
    w->data = realloc(w->data, w->cap);
}

static void pbw_raw_varint(pbw* w, uint64_t v) {
    pbw_room(w, 10);
    do {
        uint8_t b = v & 0x7f;
        v >>= 7;
        w->data[w->len++] = b | (v ? 0x80 : 0);
    } while (v);
}

int64_t pbw_new(void) { return (int64_t)(intptr_t)calloc(1, sizeof(pbw)); }

void pbw_free(int64_t h) {
    pbw* w = (pbw*)(intptr_t)h;
    if (!w) return;
    free(w->data);
    free(w);
}

void pbw_int(int64_t h, int64_t field, int64_t v) {
    pbw* w = (pbw*)(intptr_t)h;
    pbw_raw_varint(w, (uint64_t)field << 3);
    pbw_raw_varint(w, (uint64_t)v);
}

void pbw_fixed64(int64_t h, int64_t field, int64_t v) {
    pbw* w = (pbw*)(intptr_t)h;
    pbw_raw_varint(w, ((uint64_t)field << 3) | 1);
    pbw_room(w, 8);
    for (int i = 0; i < 8; i++) w->data[w->len++] = (uint64_t)v >> (8 * i);
}

void pbw_bytes(int64_t h, int64_t field, const uint8_t* b, int64_t n) {
    pbw* w = (pbw*)(intptr_t)h;
    pbw_raw_varint(w, ((uint64_t)field << 3) | 2);
    pbw_raw_varint(w, (uint64_t)n);
    pbw_room(w, n);
    memcpy(w->data + w->len, b, n);
    w->len += n;
}

void pbw_str(int64_t h, int64_t field, const char* s) {
    pbw_bytes(h, field, (const uint8_t*)s, (int64_t)strlen(s));
}

// a message inside: its fields follow, pbw_end closes it
void pbw_begin(int64_t h, int64_t field) {
    pbw* w = (pbw*)(intptr_t)h;
    pbw_raw_varint(w, ((uint64_t)field << 3) | 2);
    // room for the longest length, moved down at the end
    pbw_room(w, 5);
    w->len += 5;
    w->open[w->depth++] = w->len;
}

void pbw_end(int64_t h) {
    pbw* w = (pbw*)(intptr_t)h;
    int64_t at = w->open[--w->depth];
    int64_t n = w->len - at;
    uint8_t lb[10];
    int k = 0;
    uint64_t v = (uint64_t)n;
    do {
        uint8_t b = v & 0x7f;
        v >>= 7;
        lb[k++] = b | (v ? 0x80 : 0);
    } while (v);
    memmove(w->data + at - 5 + k, w->data + at, n);
    memcpy(w->data + at - 5, lb, k);
    w->len -= 5 - k;
}

uint8_t* pbw_data(int64_t h) { return ((pbw*)(intptr_t)h)->data; }
int64_t  pbw_len(int64_t h)  { return ((pbw*)(intptr_t)h)->len; }

// ---- reader -----------------------------------------------------------

typedef struct {
    uint8_t* data;
    int64_t  len;
} pbr;

int64_t pbr_open(const char* file) {
    FILE* fp = fopen(file, "rb");
    if (!fp) return 0;
    fseek(fp, 0, SEEK_END);
    int64_t n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    pbr* r = calloc(1, sizeof(pbr));
    r->data = malloc(n > 0 ? n : 1);
    r->len = (int64_t)fread(r->data, 1, n, fp);
    fclose(fp);
    return (int64_t)(intptr_t)r;
}

void pbr_close(int64_t h) {
    pbr* r = (pbr*)(intptr_t)h;
    if (!r) return;
    free(r->data);
    free(r);
}

static int read_varint(const uint8_t* p, const uint8_t* end,
                       const uint8_t** out, uint64_t* v) {
    uint64_t x = 0;
    for (int s = 0; s < 64 && p < end; s += 7) {
        uint8_t b = *p++;
        x |= (uint64_t)(b & 0x7f) << s;
        if (!(b & 0x80)) {
            *v = x;
            *out = p;
            return 1;
        }
    }
    return 0;
}

// the nth (from 0) field number f in [p, end): its value's place;
// a varint or fixed one comes back in *num, a length one as span
static int field_at(const uint8_t* p, const uint8_t* end, uint64_t f,
                    int64_t nth, const uint8_t** vp, int64_t* vn,
                    uint64_t* num) {
    while (p < end) {
        uint64_t key;
        if (!read_varint(p, end, &p, &key)) return 0;
        uint64_t fn = key >> 3;
        int wt = key & 7;
        const uint8_t* vstart = p;
        uint64_t v = 0;
        int64_t n = 0;
        if (wt == 0) {
            if (!read_varint(p, end, &p, &v)) return 0;
        } else if (wt == 1) {
            if (end - p < 8) return 0;
            for (int i = 0; i < 8; i++) v |= (uint64_t)p[i] << (8 * i);
            p += 8;
        } else if (wt == 2) {
            uint64_t l;
            if (!read_varint(p, end, &p, &l) || (int64_t)l > end - p)
                return 0;
            vstart = p;
            n = (int64_t)l;
            p += l;
        } else if (wt == 5) {
            if (end - p < 4) return 0;
            for (int i = 0; i < 4; i++) v |= (uint64_t)p[i] << (8 * i);
            p += 4;
        } else if (wt == 3 || wt == 4) {
            // a group's start or end: its fields are read in line
            continue;
        } else {
            return 0;
        }
        if (fn == f) {
            if (nth == 0) {
                *vp = vstart;
                *vn = n;
                *num = v;
                return 1;
            }
            nth--;
        }
    }
    return 0;
}

// a path "1.21.2.15[3].5": fields from the top, [i] the ith of a
// repeated one (0 when left out)
static int path_at(pbr* r, const char* path, const uint8_t** vp,
                   int64_t* vn, uint64_t* num) {
    const uint8_t* p = r->data;
    const uint8_t* end = r->data + r->len;
    const char* s = path;
    while (*s) {
        uint64_t f = strtoull(s, (char**)&s, 10);
        int64_t nth = 0;
        if (*s == '[') {
            nth = strtoll(s + 1, (char**)&s, 10);
            if (*s == ']') s++;
        }
        if (!field_at(p, end, f, nth, vp, vn, num)) return 0;
        if (*s == '.') {
            s++;
            p = *vp;
            end = *vp + *vn;
        }
    }
    return 1;
}

// a string field into out (cap with its 0); its length, -1 if absent
int64_t pbr_str(int64_t h, const char* path, char* out, int64_t cap) {
    pbr* r = (pbr*)(intptr_t)h;
    const uint8_t* vp;
    int64_t vn;
    uint64_t num;
    if (!r || !path_at(r, path, &vp, &vn, &num)) return -1;
    int64_t k = vn < cap - 1 ? vn : cap - 1;
    memcpy(out, vp, k);
    out[k] = 0;
    return vn;
}

// a number field (varint or fixed); fallback when absent
int64_t pbr_int(int64_t h, const char* path, int64_t fallback) {
    pbr* r = (pbr*)(intptr_t)h;
    const uint8_t* vp;
    int64_t vn;
    uint64_t num;
    if (!r || !path_at(r, path, &vp, &vn, &num)) return fallback;
    return (int64_t)num;
}

// how many of the repeated last field in a path ("1.1.2.11")
int64_t pbr_count(int64_t h, const char* path) {
    pbr* r = (pbr*)(intptr_t)h;
    if (!r) return 0;
    const char* dot = strrchr(path, '.');
    const uint8_t* p = r->data;
    const uint8_t* end = r->data + r->len;
    if (dot) {
        char head[256];
        int64_t k = dot - path;
        if (k >= (int64_t)sizeof head) return 0;
        memcpy(head, path, k);
        head[k] = 0;
        const uint8_t* vp;
        int64_t vn;
        uint64_t num;
        if (!path_at(r, head, &vp, &vn, &num)) return 0;
        p = vp;
        end = vp + vn;
    }
    uint64_t f = strtoull(dot ? dot + 1 : path, NULL, 10);
    int64_t n = 0;
    const uint8_t* vp;
    int64_t vn;
    uint64_t num;
    while (field_at(p, end, f, n, &vp, &vn, &num)) n++;
    return n;
}

// ---- text -------------------------------------------------------------

// s as a URL's query value; the length written
int64_t url_enc(const char* s, char* out, int64_t cap) {
    static const char hex[] = "0123456789ABCDEF";
    int64_t k = 0;
    for (; *s && k < cap - 4; s++) {
        unsigned char c = (unsigned char)*s;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '~') {
            out[k++] = c;
        } else {
            out[k++] = '%';
            out[k++] = hex[c >> 4];
            out[k++] = hex[c & 15];
        }
    }
    out[k] = 0;
    return k;
}

// a key's value in a file of key=value lines (Google's auth
// answers); its length, -1 when the file or the key is not there
int64_t kv_value(const char* file, const char* key, char* out, int64_t cap) {
    FILE* fp = fopen(file, "rb");
    if (!fp) return -1;
    char line[8192];
    size_t kl = strlen(key);
    int64_t got = -1;
    while (fgets(line, sizeof line, fp)) {
        if (strncmp(line, key, kl) || line[kl] != '=') continue;
        char* v = line + kl + 1;
        size_t n = strcspn(v, "\r\n");
        if ((int64_t)n > cap - 1) n = cap - 1;
        memcpy(out, v, n);
        out[n] = 0;
        got = (int64_t)n;
        break;
    }
    fclose(fp);
    return got;
}

// a number as lower hex (Google's device id is written so)
void hex64(int64_t v, char* out) {
    sprintf(out, "%llx", (unsigned long long)v);
}

// "nonce=" and 256 random bytes in URL-safe base64, no padding
void play_nonce(char* out) {
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    uint8_t r[256];
    FILE* fp = fopen("/dev/urandom", "rb");
    if (!fp || fread(r, 1, sizeof r, fp) != sizeof r)
        for (int i = 0; i < 256; i++) r[i] = (uint8_t)rand();
    if (fp) fclose(fp);
    int k = sprintf(out, "nonce=");
    int i = 0;
    for (; i + 2 < 256; i += 3) {
        uint32_t v = (r[i] << 16) | (r[i + 1] << 8) | r[i + 2];
        out[k++] = b64[(v >> 18) & 63];
        out[k++] = b64[(v >> 12) & 63];
        out[k++] = b64[(v >> 6) & 63];
        out[k++] = b64[v & 63];
    }
    // 256 = 85 x 3 + 1: one byte left, two characters
    uint32_t v = r[i] << 16;
    out[k++] = b64[(v >> 18) & 63];
    out[k++] = b64[(v >> 12) & 63];
    out[k] = 0;
}

// a picture's kind by its first bytes: 1 png, 2 jpeg, 0 neither
int64_t pic_kind(const char* file) {
    uint8_t h[4] = { 0 };
    FILE* fp = fopen(file, "rb");
    if (!fp) return 0;
    size_t n = fread(h, 1, 4, fp);
    fclose(fp);
    if (n == 4 && h[0] == 0x89 && h[1] == 'P' && h[2] == 'N' && h[3] == 'G')
        return 1;
    if (n >= 3 && h[0] == 0xFF && h[1] == 0xD8 && h[2] == 0xFF) return 2;
    return 0;
}

// Android leaves a library's dynamic section as linked; glibc makes
// its addresses absolute unless PT_DYNAMIC is read only. code that
// reads a library's tables itself (app protectors) needs Android's
static int dyn_readonly(const char* file) {
    FILE* fp = fopen(file, "r+b");
    if (!fp) return 0;
    Elf64_Ehdr eh;
    int done = 0;
    if (fread(&eh, sizeof eh, 1, fp) == 1
        && memcmp(eh.e_ident, ELFMAG, SELFMAG) == 0
        && eh.e_ident[EI_CLASS] == ELFCLASS64)
        for (int i = 0; i < eh.e_phnum; i++) {
            Elf64_Phdr ph;
            long at = (long)eh.e_phoff + (long)i * eh.e_phentsize;
            if (fseek(fp, at, SEEK_SET) || fread(&ph, sizeof ph, 1, fp) != 1)
                break;
            if (ph.p_type != PT_DYNAMIC || !(ph.p_flags & PF_W)) continue;
            ph.p_flags &= ~PF_W;
            if (!fseek(fp, at, SEEK_SET) && fwrite(&ph, sizeof ph, 1, fp) == 1)
                done = 1;
        }
    fclose(fp);
    return done;
}

// each library file in dir whose name starts with one of the
// comma separated starts ("" all): its PT_DYNAMIC read only
int64_t dyn_readonly_dir(const char* dir, const char* starts) {
    DIR* d = opendir(dir);
    if (!d) return 0;
    int64_t n = 0;
    struct dirent* e;
    char file[4096];
    while ((e = readdir(d))) {
        if (e->d_type != DT_REG || !strstr(e->d_name, ".so")) continue;
        int ok = !*starts;
        for (const char* s = starts; *s && !ok; ) {
            const char* c = strchr(s, ',');
            size_t k = c ? (size_t)(c - s) : strlen(s);
            if (k && strncmp(e->d_name, s, k) == 0) ok = 1;
            s = c ? c + 1 : s + k;
        }
        if (!ok) continue;
        snprintf(file, sizeof file, "%s/%s", dir, e->d_name);
        n += dyn_readonly(file);
    }
    closedir(d);
    return n;
}

// one TZif file under the zoneinfo folder, by its zone id
struct tz_zone {
    char* id;
    unsigned char* data;
    int size;
    int raw_ms;
};

static int tz_cmp(const void* a, const void* b) {
    return strcmp(((const struct tz_zone*)a)->id,
                  ((const struct tz_zone*)b)->id);
}

static uint32_t tz_be32(const unsigned char* p) {
    return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

// the zone's standard offset in ms: its latest non-dst type's
static int tz_raw_ms(const unsigned char* d, int n) {
    if (n < 44 || memcmp(d, "TZif", 4)) return 0;
    uint32_t isut = tz_be32(d + 20), isstd = tz_be32(d + 24);
    uint32_t leap = tz_be32(d + 28), times = tz_be32(d + 32);
    uint32_t types = tz_be32(d + 36), chars = tz_be32(d + 40);
    (void)isut; (void)isstd; (void)leap; (void)chars;
    const unsigned char* idx = d + 44 + times * 4;
    const unsigned char* ty = idx + times;
    if (ty + types * 6 > d + n) return 0;
    int raw = (int32_t)tz_be32(ty);
    for (uint32_t i = 0; i < times; i++) {
        unsigned char t = idx[i];
        if (t >= types) continue;
        const unsigned char* e = ty + t * 6;
        if (!e[4]) raw = (int32_t)tz_be32(e);
    }
    return raw * 1000;
}

static void tz_walk(const char* root, const char* rel,
                    struct tz_zone** zones, int* count, int* room) {
    char dir[4096];
    snprintf(dir, sizeof dir, "%s/%s", root, rel);
    DIR* d = opendir(*rel ? dir : root);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char id[4096];
        snprintf(id, sizeof id, "%s%s%s", rel, *rel ? "/" : "", e->d_name);
        if (strlen(id) >= 40) continue;
        char file[4096];
        snprintf(file, sizeof file, "%s/%s", root, id);
        struct stat st;
        if (stat(file, &st)) continue;
        if (S_ISDIR(st.st_mode)) {
            if (!strcmp(id, "posix") || !strcmp(id, "right")) continue;
            tz_walk(root, id, zones, count, room);
            continue;
        }
        if (!S_ISREG(st.st_mode) || st.st_size < 44) continue;
        FILE* fp = fopen(file, "rb");
        if (!fp) continue;
        unsigned char* data = malloc(st.st_size);
        int n = (int)fread(data, 1, st.st_size, fp);
        fclose(fp);
        if (n < 44 || memcmp(data, "TZif", 4)) { free(data); continue; }
        if (*count == *room) {
            *room = *room ? *room * 2 : 256;
            *zones = realloc(*zones, *room * sizeof **zones);
        }
        struct tz_zone* z = &(*zones)[(*count)++];
        z->id = strdup(id);
        z->data = data;
        z->size = n;
        z->raw_ms = tz_raw_ms(data, n);
    }
    closedir(d);
}

static void tz_put32(FILE* fp, uint32_t v) {
    unsigned char b[4] = { v >> 24, v >> 16, v >> 8, v };
    fwrite(b, 1, 4, fp);
}

// Android's tzdata (libcore ZoneInfoDB) from a zoneinfo folder:
// header, index (id, offset, length, raw offset), the TZif files,
// zone.tab. Returns the zone count, 0 when nothing was written.
int64_t tzdata_write(const char* zoneinfo, const char* out) {
    struct tz_zone* zones = NULL;
    int count = 0, room = 0;
    tz_walk(zoneinfo, "", &zones, &count, &room);
    if (!count) return 0;
    qsort(zones, count, sizeof *zones, tz_cmp);
    char version[6] = "0000a";
    char file[4096];
    snprintf(file, sizeof file, "%s/tzdata.zi", zoneinfo);
    FILE* fp = fopen(file, "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof line, fp) && !strncmp(line, "# version ", 10))
            memcpy(version, line + 10, 5);
        fclose(fp);
    }
    snprintf(file, sizeof file, "%s/zone.tab", zoneinfo);
    unsigned char* tab = NULL;
    long tab_n = 0;
    fp = fopen(file, "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        tab_n = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        tab = malloc(tab_n + 1);
        tab_n = (long)fread(tab, 1, tab_n, fp);
        fclose(fp);
    }
    fp = fopen(out, "wb");
    if (!fp) return 0;
    uint32_t index_off = 24, data_off = index_off + count * 52;
    uint32_t data_n = 0;
    for (int i = 0; i < count; i++) data_n += zones[i].size;
    fwrite("tzdata", 1, 6, fp);
    fwrite(version, 1, 5, fp);
    fputc(0, fp);
    tz_put32(fp, index_off);
    tz_put32(fp, data_off);
    tz_put32(fp, data_off + data_n);
    uint32_t at = 0;
    for (int i = 0; i < count; i++) {
        char id[40] = {0};
        strncpy(id, zones[i].id, sizeof id - 1);
        fwrite(id, 1, sizeof id, fp);
        tz_put32(fp, at);
        tz_put32(fp, zones[i].size);
        tz_put32(fp, (uint32_t)zones[i].raw_ms);
        at += zones[i].size;
    }
    for (int i = 0; i < count; i++)
        fwrite(zones[i].data, 1, zones[i].size, fp);
    if (tab) fwrite(tab, 1, tab_n, fp);
    fclose(fp);
    for (int i = 0; i < count; i++) { free(zones[i].id); free(zones[i].data); }
    free(zones);
    free(tab);
    return count;
}
