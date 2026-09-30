#include "composer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct GifAnim {
    int w, h, n, cap;
    uint8_t** frames;
    int* delays;
};

typedef struct {
    const uint8_t* p;
    long n, at;
} Rd;

static int rd8(Rd* r) { return r->at < r->n ? r->p[r->at++] : -1; }
static int rd16(Rd* r) { int a = rd8(r), b = rd8(r); return a | (b << 8); }

// the data sub-blocks of an image, joined
static uint8_t* sub_blocks(Rd* r, long* len) {
    long cap = 4096, n = 0;
    uint8_t* d = malloc((size_t)cap);
    for (;;) {
        int k = rd8(r);
        if (k <= 0) break;
        if (n + k > cap) { cap = (n + k) * 2; d = realloc(d, (size_t)cap); }
        if (r->at + k > r->n) break;
        memcpy(d + n, r->p + r->at, (size_t)k);
        r->at += k;
        n += k;
    }
    *len = n;
    return d;
}

// lzw codes to color indices; returns the count written
static long lzw(const uint8_t* s, long sn, int min, uint8_t* out, long want) {
    enum { MAXC = 4096 };
    static uint16_t prefix[MAXC];
    static uint8_t suffix[MAXC], stack[MAXC + 1];
    int clear = 1 << min, end = clear + 1, size = min + 1, next = clear + 2, old = -1;
    uint8_t first = 0;
    long o = 0, bitpos = 0;
    for (int i = 0; i < clear; i++) { prefix[i] = 0; suffix[i] = (uint8_t)i; }
    while (o < want) {
        long byte = bitpos >> 3;
        if (byte >= sn) break;
        uint32_t v = s[byte] | (byte + 1 < sn ? s[byte + 1] << 8 : 0) | (byte + 2 < sn ? s[byte + 2] << 16 : 0);
        int code = (int)((v >> (bitpos & 7)) & ((1u << size) - 1));
        bitpos += size;
        if (code == clear) { size = min + 1; next = clear + 2; old = -1; continue; }
        if (code == end) break;
        if (old < 0) {
            out[o++] = suffix[code];
            first = suffix[code];
            old = code;
            continue;
        }
        int in = code, sp = 0;
        if (code >= next) { stack[sp++] = first; code = old; }
        while (code >= clear && sp < MAXC) { stack[sp++] = suffix[code]; code = prefix[code]; }
        first = suffix[code];
        stack[sp++] = first;
        while (sp > 0 && o < want) out[o++] = stack[--sp];
        if (next < MAXC) {
            prefix[next] = (uint16_t)old;
            suffix[next] = first;
            next++;
            if (next == (1 << size) && size < 12) size++;
        }
        old = in;
    }
    return o;
}

static void push_frame(GifAnim* g, const uint8_t* canvas, int delay) {
    if (g->n == g->cap) {
        g->cap = g->cap ? g->cap * 2 : 16;
        g->frames = realloc(g->frames, sizeof(uint8_t*) * (size_t)g->cap);
        g->delays = realloc(g->delays, sizeof(int) * (size_t)g->cap);
    }
    size_t sz = (size_t)g->w * g->h * 4;
    g->frames[g->n] = malloc(sz);
    memcpy(g->frames[g->n], canvas, sz);
    // browsers show a zero or tiny delay as 100 ms
    g->delays[g->n] = delay < 20 ? 100 : delay;
    g->n++;
}

GifAnim* gif_open(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* buf = malloc((size_t)n);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(buf); return NULL; }
    fclose(f);
    Rd r = { buf, n, 0 };
    if (n < 13 || memcmp(buf, "GIF", 3) != 0) { free(buf); return NULL; }
    r.at = 6;
    GifAnim* g = calloc(1, sizeof(GifAnim));
    g->w = rd16(&r);
    g->h = rd16(&r);
    int flags = rd8(&r);
    int bg = rd8(&r);
    rd8(&r);
    uint8_t gct[768] = {0};
    int gct_n = 0;
    if (flags & 0x80) {
        gct_n = 1 << ((flags & 7) + 1);
        for (int i = 0; i < gct_n * 3; i++) gct[i] = (uint8_t)rd8(&r);
    }
    (void)bg;
    size_t sz = (size_t)g->w * g->h * 4;
    uint8_t* canvas = calloc(1, sz);
    uint8_t* saved = malloc(sz);
    int delay = 100, trans = -1, disposal = 0;
    for (;;) {
        int b = rd8(&r);
        if (b < 0 || b == 0x3B) break;
        if (b == 0x21) {
            int label = rd8(&r);
            if (label == 0xF9) {
                rd8(&r);
                int pf = rd8(&r);
                delay = rd16(&r) * 10;
                int ti = rd8(&r);
                rd8(&r);
                disposal = (pf >> 2) & 7;
                trans = (pf & 1) ? ti : -1;
            } else {
                long l;
                free(sub_blocks(&r, &l));
            }
            continue;
        }
        if (b != 0x2C) break;
        int x = rd16(&r), y = rd16(&r), fw = rd16(&r), fh = rd16(&r);
        int lf = rd8(&r);
        uint8_t lct[768];
        const uint8_t* ct = gct;
        if (lf & 0x80) {
            int ln = 1 << ((lf & 7) + 1);
            for (int i = 0; i < ln * 3; i++) lct[i] = (uint8_t)rd8(&r);
            ct = lct;
        }
        int interlace = (lf >> 6) & 1;
        int min = rd8(&r);
        long len;
        uint8_t* data = sub_blocks(&r, &len);
        long want = (long)fw * fh;
        uint8_t* idx = calloc(1, (size_t)(want > 0 ? want : 1));
        if (min >= 2 && min <= 8) lzw(data, len, min, idx, want);
        free(data);
        if (disposal == 3) memcpy(saved, canvas, sz);
        // interlaced rows come in four passes
        static const int start[4] = { 0, 4, 2, 1 }, step[4] = { 8, 8, 4, 2 };
        int row = 0;
        for (int pass = 0; pass < (interlace ? 4 : 1); pass++) {
            for (int yy = interlace ? start[pass] : 0; yy < fh; yy += interlace ? step[pass] : 1, row++) {
                int cy = y + yy;
                if (cy < 0 || cy >= g->h) continue;
                for (int xx = 0; xx < fw; xx++) {
                    int cx = x + xx;
                    if (cx < 0 || cx >= g->w) continue;
                    int c = idx[(long)row * fw + xx];
                    if (c == trans) continue;
                    uint8_t* px = canvas + ((size_t)cy * g->w + cx) * 4;
                    px[0] = ct[c * 3];
                    px[1] = ct[c * 3 + 1];
                    px[2] = ct[c * 3 + 2];
                    px[3] = 255;
                }
            }
        }
        free(idx);
        push_frame(g, canvas, delay);
        if (disposal == 2) {
            for (int yy = 0; yy < fh; yy++) {
                int cy = y + yy;
                if (cy < 0 || cy >= g->h) continue;
                for (int xx = 0; xx < fw; xx++) {
                    int cx = x + xx;
                    if (cx < 0 || cx >= g->w) continue;
                    memset(canvas + ((size_t)cy * g->w + cx) * 4, 0, 4);
                }
            }
        } else if (disposal == 3)
            memcpy(canvas, saved, sz);
        delay = 100;
        trans = -1;
        disposal = 0;
    }
    free(canvas);
    free(saved);
    free(buf);
    if (g->n == 0) { gif_free(g); return NULL; }
    return g;
}

void gif_free(GifAnim* g) {
    if (!g) return;
    for (int i = 0; i < g->n; i++) free(g->frames[i]);
    free(g->frames);
    free(g->delays);
    free(g);
}

int gif_width(GifAnim* g)  { return g->w; }
int gif_height(GifAnim* g) { return g->h; }
int gif_count(GifAnim* g)  { return g->n; }
const uint8_t* gif_frame(GifAnim* g, int i) { return g->frames[i]; }
int gif_delay(GifAnim* g, int i) { return g->delays[i]; }
