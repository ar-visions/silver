#define _POSIX_C_SOURCE 200809L
#include "ai.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- the zip: stored entries only (torch.save writes them so) ----
typedef struct { char* name; int64_t off, size; int method; } ZEntry;

// ---- pickle values ----
enum { PV_NONE, PV_INT, PV_FLOAT, PV_STR, PV_TUPLE, PV_LIST, PV_DICT, PV_GLOBAL,
       PV_STORAGE, PV_TENSOR, PV_MARK, PV_OTHER, PV_BOOL };

typedef struct PV PV;
struct PV {
    int kind;
    int64_t i;
    double f;
    char* s;                 // str, global "module.name", storage key
    int n, cap;              // tuple/list items, dict pairs (2n)
    PV** items;
    // storage: dtype; tensor: storage, offset, shape, strides
    int dtype;
    PV* storage;
    int64_t offset, shape[8], stride[8];
    int ndim;
};

typedef struct { char* name; PV* t; } PthTensor;

struct PthFile {
    FILE* fp;
    char* prefix;            // "<archive>/"
    ZEntry* ents; int nents;
    PthTensor* ts; int nts, capts;
    PV** all; int nall, capall;
};

static PV* pv(PthFile* p, int kind) {
    PV* v = calloc(1, sizeof(PV));
    v->kind = kind;
    if (p->nall == p->capall) { p->capall = p->capall ? p->capall * 2 : 256; p->all = realloc(p->all, sizeof(PV*) * p->capall); }
    p->all[p->nall++] = v;
    return v;
}
static void pv_push(PV* c, PV* x) {
    if (c->n == c->cap) { c->cap = c->cap ? c->cap * 2 : 8; c->items = realloc(c->items, sizeof(PV*) * c->cap); }
    c->items[c->n++] = x;
}

static uint32_t le32(const uint8_t* b) { return b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24; }
static uint16_t le16(const uint8_t* b) { return (uint16_t)(b[0] | b[1] << 8); }
static uint64_t le64(const uint8_t* b) { return le32(b) | (uint64_t)le32(b + 4) << 32; }

static int zip_read(PthFile* p) {
    fseek(p->fp, 0, SEEK_END);
    int64_t size = ftell(p->fp);
    int64_t tail = size < 70000 ? size : 70000;
    uint8_t* buf = malloc((size_t)tail);
    fseek(p->fp, size - tail, SEEK_SET);
    if (fread(buf, 1, (size_t)tail, p->fp) != (size_t)tail) { free(buf); return 0; }
    int64_t eocd = -1;
    for (int64_t i = tail - 22; i >= 0; i--)
        if (le32(buf + i) == 0x06054b50) { eocd = i; break; }
    if (eocd < 0) { free(buf); return 0; }
    int64_t n = le16(buf + eocd + 10);
    int64_t cd = le32(buf + eocd + 16), cdsize = le32(buf + eocd + 12);
    // zip64: the locator sits just before the end record
    if (eocd >= 20 && le32(buf + eocd - 20) == 0x07064b50) {
        int64_t z64 = (int64_t)le64(buf + eocd - 20 + 8);
        uint8_t zb[56];
        fseek(p->fp, z64, SEEK_SET);
        if (fread(zb, 1, 56, p->fp) == 56 && le32(zb) == 0x06064b50) {
            n = (int64_t)le64(zb + 32);
            cdsize = (int64_t)le64(zb + 40);
            cd = (int64_t)le64(zb + 48);
        }
    }
    free(buf);
    uint8_t* c = malloc((size_t)cdsize);
    fseek(p->fp, cd, SEEK_SET);
    if (fread(c, 1, (size_t)cdsize, p->fp) != (size_t)cdsize) { free(c); return 0; }
    p->ents = calloc((size_t)n, sizeof(ZEntry));
    int64_t at = 0;
    for (int64_t k = 0; k < n && at + 46 <= cdsize; k++) {
        if (le32(c + at) != 0x02014b50) break;
        int method = le16(c + at + 10);
        int64_t csize = le32(c + at + 20), usize = le32(c + at + 24);
        int nl = le16(c + at + 28), xl = le16(c + at + 30), cl = le16(c + at + 32);
        int64_t loc = le32(c + at + 42);
        // zip64 extra field: the 0xffffffff values follow in order
        const uint8_t* x = c + at + 46 + nl;
        for (int xo = 0; xo + 4 <= xl;) {
            int id = le16(x + xo), sz = le16(x + xo + 2);
            if (id == 1) {
                int q = xo + 4;
                if (usize == 0xffffffff) { usize = (int64_t)le64(x + q); q += 8; }
                if (csize == 0xffffffff) { csize = (int64_t)le64(x + q); q += 8; }
                if (loc == 0xffffffff) { loc = (int64_t)le64(x + q); q += 8; }
            }
            xo += 4 + sz;
        }
        ZEntry* e = &p->ents[p->nents++];
        e->name = malloc((size_t)nl + 1);
        memcpy(e->name, c + at + 46, (size_t)nl);
        e->name[nl] = 0;
        e->size = usize;
        e->method = method;
        // the data starts after the local header's own name and extra
        uint8_t lh[30];
        fseek(p->fp, loc, SEEK_SET);
        if (fread(lh, 1, 30, p->fp) == 30)
            e->off = loc + 30 + le16(lh + 26) + le16(lh + 28);
        at += 46 + nl + xl + cl;
    }
    free(c);
    return p->nents > 0;
}

static ZEntry* entry(PthFile* p, const char* tail) {
    for (int i = 0; i < p->nents; i++) {
        const char* s = strrchr(p->ents[i].name, '/');
        // data.pkl sits one folder deep; storages two ("<a>/data/<k>")
        if (!strcmp(tail, "data.pkl")) {
            if (s && !strcmp(s + 1, "data.pkl")) {
                size_t pl = (size_t)(s - p->ents[i].name) + 1;
                p->prefix = malloc(pl + 1);
                memcpy(p->prefix, p->ents[i].name, pl);
                p->prefix[pl] = 0;
                return &p->ents[i];
            }
        } else {
            char want[512];
            snprintf(want, sizeof(want), "%sdata/%s", p->prefix ? p->prefix : "", tail);
            if (!strcmp(p->ents[i].name, want)) return &p->ents[i];
        }
    }
    return NULL;
}

static int storage_dtype(const char* g) {
    if (strstr(g, "FloatStorage")) return 0;
    if (strstr(g, "HalfStorage")) return 1;
    if (strstr(g, "BFloat16Storage")) return 2;
    if (strstr(g, "LongStorage")) return 3;
    if (strstr(g, "IntStorage")) return 4;
    if (strstr(g, "ByteStorage") || strstr(g, "BoolStorage")) return 5;
    if (strstr(g, "DoubleStorage")) return 6;
    return -1;
}

static void collect(PthFile* p, PV* v, const char* path) {
    if (!v) return;
    if (v->kind == PV_TENSOR) {
        if (p->nts == p->capts) { p->capts = p->capts ? p->capts * 2 : 256; p->ts = realloc(p->ts, sizeof(PthTensor) * p->capts); }
        p->ts[p->nts].name = strdup(path);
        p->ts[p->nts].t = v;
        p->nts++;
        return;
    }
    if (v->kind == PV_DICT) {
        for (int k = 0; k + 1 < v->n; k += 2) {
            PV* key = v->items[k];
            char sub[1024];
            if (key->kind == PV_STR) snprintf(sub, sizeof(sub), "%s%s%s", path, *path ? "." : "", key->s);
            else if (key->kind == PV_INT) snprintf(sub, sizeof(sub), "%s%s%lld", path, *path ? "." : "", (long long)key->i);
            else continue;
            collect(p, v->items[k + 1], sub);
        }
    }
    if (v->kind == PV_LIST || v->kind == PV_TUPLE)
        for (int k = 0; k < v->n; k++) {
            char sub[1024];
            snprintf(sub, sizeof(sub), "%s%s%d", path, *path ? "." : "", k);
            collect(p, v->items[k], sub);
        }
}

// ---- the pickle machine: the opcodes torch.save uses ----
static PV* unpickle(PthFile* p, const uint8_t* b, int64_t n) {
    PV** st = malloc(sizeof(PV*) * 65536);
    int sp = 0;
    PV** memo = calloc(1 << 20, sizeof(PV*));
    int64_t at = 0, memo_n = 0;
    PV* result = NULL;
#define PUSH(x) do { if (sp < 65536) st[sp++] = (x); } while (0)
#define POP() (sp > 0 ? st[--sp] : NULL)
    while (at < n) {
        int op = b[at++];
        switch (op) {
        case 0x80: at++; break;                              // PROTO
        case 0x95: at += 8; break;                           // FRAME
        case '}': PUSH(pv(p, PV_DICT)); break;
        case ']': PUSH(pv(p, PV_LIST)); break;
        case ')': PUSH(pv(p, PV_TUPLE)); break;
        case '(': PUSH(pv(p, PV_MARK)); break;
        case 'N': PUSH(pv(p, PV_NONE)); break;
        case 0x88: case 0x89: { PV* v = pv(p, PV_BOOL); v->i = op == 0x88; PUSH(v); break; }
        case 'J': { PV* v = pv(p, PV_INT); v->i = (int32_t)le32(b + at); at += 4; PUSH(v); break; }
        case 'K': { PV* v = pv(p, PV_INT); v->i = b[at++]; PUSH(v); break; }
        case 'M': { PV* v = pv(p, PV_INT); v->i = le16(b + at); at += 2; PUSH(v); break; }
        case 0x8a: {                                         // LONG1
            int k = b[at++];
            int64_t v0 = 0;
            for (int q = 0; q < k && q < 8; q++) v0 |= (int64_t)b[at + q] << (8 * q);
            if (k > 0 && k < 8 && (b[at + k - 1] & 0x80)) v0 -= (int64_t)1 << (8 * k);
            at += k;
            PV* v = pv(p, PV_INT); v->i = v0; PUSH(v); break;
        }
        case 'G': {                                          // BINFLOAT, big-endian
            uint64_t u = 0;
            for (int q = 0; q < 8; q++) u = u << 8 | b[at + q];
            at += 8;
            PV* v = pv(p, PV_FLOAT); memcpy(&v->f, &u, 8); PUSH(v); break;
        }
        case 'X': case 0x8c: case 'B': case 'C': case 0x8e: {   // strings and bytes
            int64_t k;
            if (op == 'X') { k = le32(b + at); at += 4; }
            else if (op == 0x8e) { k = (int64_t)le64(b + at); at += 8; }
            else if (op == 'B') { k = le32(b + at); at += 4; }
            else { k = b[at++]; }
            PV* v = pv(p, PV_STR);
            v->s = malloc((size_t)k + 1);
            memcpy(v->s, b + at, (size_t)k);
            v->s[k] = 0;
            at += k;
            PUSH(v); break;
        }
        case 'c': {                                          // GLOBAL "module\nname\n"
            const char* m = (const char*)b + at;
            const char* e1 = memchr(m, '\n', (size_t)(n - at));
            const char* e2 = e1 ? memchr(e1 + 1, '\n', (size_t)(n - (e1 + 1 - (const char*)b))) : NULL;
            if (!e2) goto done;
            PV* v = pv(p, PV_GLOBAL);
            v->s = malloc((size_t)(e2 - m) + 1);
            memcpy(v->s, m, (size_t)(e1 - m));
            v->s[e1 - m] = '.';
            memcpy(v->s + (e1 - m) + 1, e1 + 1, (size_t)(e2 - e1 - 1));
            v->s[e2 - m] = 0;
            at = e2 + 1 - (const char*)b;
            PUSH(v); break;
        }
        case 0x93: {                                         // STACK_GLOBAL
            PV* nm = POP(); PV* md = POP();
            PV* v = pv(p, PV_GLOBAL);
            size_t l = strlen(md && md->s ? md->s : "") + strlen(nm && nm->s ? nm->s : "") + 2;
            v->s = malloc(l);
            snprintf(v->s, l, "%s.%s", md && md->s ? md->s : "", nm && nm->s ? nm->s : "");
            PUSH(v); break;
        }
        case 'q': memo[b[at++]] = sp ? st[sp - 1] : NULL; break;
        case 'r': { uint32_t k = le32(b + at); at += 4; if (k < (1u << 20)) memo[k] = sp ? st[sp - 1] : NULL; break; }
        case 0x94:                                           // MEMOIZE: the next slot
            if (memo_n < (1 << 20)) memo[memo_n++] = sp ? st[sp - 1] : NULL;
            break;
        case 'h': PUSH(memo[b[at++]]); break;
        case 'j': { uint32_t k = le32(b + at); at += 4; PUSH(k < (1u << 20) ? memo[k] : NULL); break; }
        case 't': {                                          // TUPLE from mark
            int m = sp - 1;
            while (m >= 0 && !(st[m] && st[m]->kind == PV_MARK)) m--;
            PV* v = pv(p, PV_TUPLE);
            for (int q = m + 1; q < sp; q++) pv_push(v, st[q]);
            sp = m < 0 ? 0 : m;
            PUSH(v); break;
        }
        case 0x85: case 0x86: case 0x87: {                   // TUPLE1..3
            int k = op - 0x84;
            PV* v = pv(p, PV_TUPLE);
            for (int q = sp - k; q < sp; q++) pv_push(v, st[q]);
            sp -= k;
            PUSH(v); break;
        }
        case 'Q': {                                          // BINPERSID
            PV* pid = POP();
            PV* v = pv(p, PV_STORAGE);
            // ('storage', <type>, key, location, numel)
            if (pid && pid->kind == PV_TUPLE && pid->n >= 3) {
                v->dtype = pid->items[1] && pid->items[1]->s ? storage_dtype(pid->items[1]->s) : -1;
                v->s = pid->items[2]->s ? strdup(pid->items[2]->s) : NULL;
            }
            PUSH(v); break;
        }
        case 'R': {                                          // REDUCE
            PV* args = POP(); PV* fn = POP();
            PV* out = NULL;
            const char* g = fn && fn->kind == PV_GLOBAL ? fn->s : "";
            if (strstr(g, "_rebuild_tensor") && args && args->n >= 4) {
                out = pv(p, PV_TENSOR);
                out->storage = args->items[0];
                out->offset = args->items[1]->i;
                PV* sz = args->items[2]; PV* sd = args->items[3];
                out->ndim = sz->n < 8 ? sz->n : 8;
                for (int q = 0; q < out->ndim; q++) {
                    out->shape[q] = sz->items[q]->i;
                    out->stride[q] = q < sd->n ? sd->items[q]->i : 0;
                }
            } else if (strstr(g, "_rebuild_parameter") && args && args->n >= 1) {
                out = args->items[0];
            } else if (strstr(g, "OrderedDict") || strstr(g, "builtins.dict")) {
                out = pv(p, PV_DICT);
            } else {
                out = pv(p, PV_OTHER);
            }
            PUSH(out); break;
        }
        case 'b': POP(); break;                              // BUILD: state ignored
        case 's': {                                          // SETITEM
            PV* v = POP(); PV* k = POP(); PV* d = sp ? st[sp - 1] : NULL;
            if (d && d->kind == PV_DICT) { pv_push(d, k); pv_push(d, v); }
            break;
        }
        case 'u': {                                          // SETITEMS
            int m = sp - 1;
            while (m >= 0 && !(st[m] && st[m]->kind == PV_MARK)) m--;
            PV* d = m > 0 ? st[m - 1] : NULL;
            for (int q = m + 1; q + 1 < sp; q += 2)
                if (d && d->kind == PV_DICT) { pv_push(d, st[q]); pv_push(d, st[q + 1]); }
            sp = m < 0 ? 0 : m;
            break;
        }
        case 'a': { PV* v = POP(); PV* l = sp ? st[sp - 1] : NULL; if (l) pv_push(l, v); break; }
        case 'e': {                                          // APPENDS
            int m = sp - 1;
            while (m >= 0 && !(st[m] && st[m]->kind == PV_MARK)) m--;
            PV* l = m > 0 ? st[m - 1] : NULL;
            for (int q = m + 1; q < sp; q++) if (l) pv_push(l, st[q]);
            sp = m < 0 ? 0 : m;
            break;
        }
        case '0': POP(); break;                              // POP
        case '.': result = POP(); goto done;                 // STOP
        default:
            fprintf(stderr, "pth: pickle opcode 0x%02x not read\n", op);
            goto done;
        }
    }
done:
    free(st);
    free(memo);
    return result;
#undef PUSH
#undef POP
}

PthFile* pthf_open(const char* path) {
    PthFile* p = calloc(1, sizeof(PthFile));
    p->fp = fopen(path, "rb");
    if (!p->fp || !zip_read(p)) { pthf_close(p); return NULL; }
    ZEntry* pk = entry(p, "data.pkl");
    if (!pk || pk->method != 0) { pthf_close(p); return NULL; }
    uint8_t* b = malloc((size_t)pk->size);
    fseek(p->fp, pk->off, SEEK_SET);
    if (fread(b, 1, (size_t)pk->size, p->fp) != (size_t)pk->size) { free(b); pthf_close(p); return NULL; }
    PV* root = unpickle(p, b, pk->size);
    free(b);
    collect(p, root, "");
    return p;
}

void pthf_close(PthFile* p) {
    if (!p) return;
    if (p->fp) fclose(p->fp);
    for (int i = 0; i < p->nents; i++) free(p->ents[i].name);
    free(p->ents);
    for (int i = 0; i < p->nts; i++) free(p->ts[i].name);
    free(p->ts);
    for (int i = 0; i < p->nall; i++) { free(p->all[i]->s); free(p->all[i]->items); free(p->all[i]); }
    free(p->all);
    free(p->prefix);
    free(p);
}

int pthf_count(PthFile* p) { return p->nts; }
const char* pthf_name(PthFile* p, int i) { return p->ts[i].name; }
int pthf_find(PthFile* p, const char* name) {
    for (int i = 0; i < p->nts; i++) if (!strcmp(p->ts[i].name, name)) return i;
    return -1;
}
int pthf_dtype(PthFile* p, int i) { PV* t = p->ts[i].t; return t->storage ? t->storage->dtype : -1; }
int pthf_ndim(PthFile* p, int i) { return p->ts[i].t->ndim; }
int64_t pthf_dim(PthFile* p, int i, int k) { return k < p->ts[i].t->ndim ? p->ts[i].t->shape[k] : 1; }
int64_t pthf_numel(PthFile* p, int i) {
    int64_t n = 1;
    for (int k = 0; k < p->ts[i].t->ndim; k++) n *= p->ts[i].t->shape[k];
    return n;
}

static float half_f(uint16_t h) {
    uint32_t s = (h >> 15) & 1, e = (h >> 10) & 31, m = h & 1023, u;
    if (e == 0) {
        if (m == 0) u = s << 31;
        else { e = 127 - 15 + 1; while (!(m & 1024)) { m <<= 1; e--; } m &= 1023; u = s << 31 | e << 23 | m << 13; }
    } else if (e == 31) u = s << 31 | 0x7f800000 | m << 13;
    else u = s << 31 | (e - 15 + 127) << 23 | m << 13;
    float f; memcpy(&f, &u, 4); return f;
}

int pthf_read_f32(PthFile* p, int i, float* out) {
    PV* t = p->ts[i].t;
    if (!t->storage || !t->storage->s) return 0;
    ZEntry* e = entry(p, t->storage->s);
    if (!e || e->method != 0) return 0;
    int dt = t->storage->dtype;
    int es = dt == 0 || dt == 4 ? 4 : dt == 1 || dt == 2 ? 2 : dt == 3 || dt == 6 ? 8 : dt == 5 ? 1 : 0;
    if (!es) return 0;
    int64_t n = pthf_numel(p, i);
    // row-major and dense: one read of the whole run, then convert
    int dense = 1;
    int64_t want = 1;
    for (int d = t->ndim - 1; d >= 0; d--) {
        if (t->shape[d] != 1 && t->stride[d] != want) dense = 0;
        want *= t->shape[d];
    }
    if (dense) {
        if ((t->offset + n) * es > e->size) return 0;
        uint8_t* raw = malloc((size_t)(n * es));
        fseek(p->fp, e->off + t->offset * es, SEEK_SET);
        int ok = fread(raw, 1, (size_t)(n * es), p->fp) == (size_t)(n * es);
        for (int64_t k = 0; ok && k < n; k++) {
            const uint8_t* q = raw + k * es;
            float v;
            switch (dt) {
            case 0: memcpy(&v, q, 4); break;
            case 1: v = half_f(le16(q)); break;
            case 2: { uint32_t u = (uint32_t)le16(q) << 16; memcpy(&v, &u, 4); break; }
            case 3: v = (float)(int64_t)le64(q); break;
            case 4: v = (float)(int32_t)le32(q); break;
            case 5: v = q[0]; break;
            default: { double dd; memcpy(&dd, q, 8); v = (float)dd; }
            }
            out[k] = v;
        }
        free(raw);
        return ok;
    }
    // the element offsets through the strides, in row-major order
    int64_t idx[8] = {0};
    uint8_t one[8];
    for (int64_t k = 0; k < n; k++) {
        int64_t off = t->offset;
        for (int d = 0; d < t->ndim; d++) off += idx[d] * t->stride[d];
        if ((off + 1) * es > e->size) return 0;
        fseek(p->fp, e->off + off * es, SEEK_SET);
        if (fread(one, 1, (size_t)es, p->fp) != (size_t)es) return 0;
        float v;
        switch (dt) {
        case 0: memcpy(&v, one, 4); break;
        case 1: v = half_f(le16(one)); break;
        case 2: { uint32_t u = (uint32_t)le16(one) << 16; memcpy(&v, &u, 4); break; }
        case 3: v = (float)(int64_t)le64(one); break;
        case 4: v = (float)(int32_t)le32(one); break;
        case 5: v = one[0]; break;
        default: { double d; memcpy(&d, one, 8); v = (float)d; }
        }
        out[k] = v;
        for (int d = t->ndim - 1; d >= 0; d--) {
            if (++idx[d] < t->shape[d]) break;
            idx[d] = 0;
        }
    }
    return 1;
}

// ---- c[m, n] = a[m, k] . b[n, k] (+ bias[n]): every row of a
// against every row of b, on every core, avx2 + fma ----
#include <pthread.h>

typedef struct { const float *a, *b, *bias; float* c; int m, n, k, n0, n1; } GemmJob;

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>

__attribute__((target("avx2,fma")))
static float hsum8(__m256 v) {
    __m128 lo = _mm256_castps256_ps128(v), hi = _mm256_extractf128_ps(v, 1);
    lo = _mm_add_ps(lo, hi);
    lo = _mm_add_ps(lo, _mm_movehl_ps(lo, lo));
    lo = _mm_add_ss(lo, _mm_shuffle_ps(lo, lo, 1));
    return _mm_cvtss_f32(lo);
}

__attribute__((target("avx2,fma")))
static void* gemm_part(void* arg) {
    GemmJob* j = arg;
    int k = j->k, k8 = k & ~7;
    // 4 rows of a against 4 rows of b: 16 dot products share loads
    for (int i = 0; i < j->m; i += 4) {
        int ri = j->m - i < 4 ? j->m - i : 4;
        for (int o = j->n0; o < j->n1; o += 4) {
            int ro = j->n1 - o < 4 ? j->n1 - o : 4;
            __m256 acc[4][4];
            for (int x = 0; x < 4; x++) for (int y = 0; y < 4; y++) acc[x][y] = _mm256_setzero_ps();
            const float* ar[4]; const float* br[4];
            for (int x = 0; x < 4; x++) ar[x] = j->a + (size_t)(i + (x < ri ? x : 0)) * k;
            for (int y = 0; y < 4; y++) br[y] = j->b + (size_t)(o + (y < ro ? y : 0)) * k;
            for (int q = 0; q < k8; q += 8) {
                __m256 bv0 = _mm256_loadu_ps(br[0] + q), bv1 = _mm256_loadu_ps(br[1] + q);
                __m256 bv2 = _mm256_loadu_ps(br[2] + q), bv3 = _mm256_loadu_ps(br[3] + q);
                for (int x = 0; x < 4; x++) {
                    __m256 av = _mm256_loadu_ps(ar[x] + q);
                    acc[x][0] = _mm256_fmadd_ps(av, bv0, acc[x][0]);
                    acc[x][1] = _mm256_fmadd_ps(av, bv1, acc[x][1]);
                    acc[x][2] = _mm256_fmadd_ps(av, bv2, acc[x][2]);
                    acc[x][3] = _mm256_fmadd_ps(av, bv3, acc[x][3]);
                }
            }
            for (int x = 0; x < ri; x++)
                for (int y = 0; y < ro; y++) {
                    float s = hsum8(acc[x][y]);
                    for (int q = k8; q < k; q++) s += ar[x][q] * br[y][q];
                    if (j->bias) s += j->bias[o + y];
                    j->c[(size_t)(i + x) * j->n + o + y] = s;
                }
        }
    }
    return NULL;
}
#elif defined(__aarch64__)
#include <arm_neon.h>

// the avx2 kernel's 4x4 tiling, 4 floats a step
static void* gemm_part(void* arg) {
    GemmJob* j = arg;
    int k = j->k, k4 = k & ~3;
    for (int i = 0; i < j->m; i += 4) {
        int ri = j->m - i < 4 ? j->m - i : 4;
        for (int o = j->n0; o < j->n1; o += 4) {
            int ro = j->n1 - o < 4 ? j->n1 - o : 4;
            float32x4_t acc[4][4];
            for (int x = 0; x < 4; x++) for (int y = 0; y < 4; y++) acc[x][y] = vdupq_n_f32(0.0f);
            const float* ar[4]; const float* br[4];
            for (int x = 0; x < 4; x++) ar[x] = j->a + (size_t)(i + (x < ri ? x : 0)) * k;
            for (int y = 0; y < 4; y++) br[y] = j->b + (size_t)(o + (y < ro ? y : 0)) * k;
            for (int q = 0; q < k4; q += 4) {
                float32x4_t bv0 = vld1q_f32(br[0] + q), bv1 = vld1q_f32(br[1] + q);
                float32x4_t bv2 = vld1q_f32(br[2] + q), bv3 = vld1q_f32(br[3] + q);
                for (int x = 0; x < 4; x++) {
                    float32x4_t av = vld1q_f32(ar[x] + q);
                    acc[x][0] = vfmaq_f32(acc[x][0], av, bv0);
                    acc[x][1] = vfmaq_f32(acc[x][1], av, bv1);
                    acc[x][2] = vfmaq_f32(acc[x][2], av, bv2);
                    acc[x][3] = vfmaq_f32(acc[x][3], av, bv3);
                }
            }
            for (int x = 0; x < ri; x++)
                for (int y = 0; y < ro; y++) {
                    float s = vaddvq_f32(acc[x][y]);
                    for (int q = k4; q < k; q++) s += ar[x][q] * br[y][q];
                    if (j->bias) s += j->bias[o + y];
                    j->c[(size_t)(i + x) * j->n + o + y] = s;
                }
        }
    }
    return NULL;
}
#else
#error "ai_gemm_nt: no kernel for this architecture"
#endif

void ai_gemm_nt(const float* a, const float* b, const float* bias, float* c, int m, int n, int k) {
    long work = (long)m * n * k;
    int th = work < (1L << 22) ? 1 : 32;
    if (th > n / 4) th = n / 4 > 0 ? n / 4 : 1;
    GemmJob jobs[32];
    pthread_t tid[32];
    int per = ((n + th - 1) / th + 3) & ~3;
    int used = 0;
    for (int t = 0; t < th; t++) {
        int n0 = t * per, n1 = n0 + per < n ? n0 + per : n;
        if (n0 >= n) break;
        jobs[t] = (GemmJob){ a, b, bias, c, m, n, k, n0, n1 };
        used++;
    }
    for (int t = 1; t < used; t++) pthread_create(&tid[t], NULL, gemm_part, &jobs[t]);
    gemm_part(&jobs[0]);
    for (int t = 1; t < used; t++) pthread_join(tid[t], NULL);
}
