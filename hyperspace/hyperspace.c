// the eye networks run on the CPU: find, target and the eye stage,
// as base.py trains them (batch norms folded in by its export)
// clock_gettime: hidden under -std=c11 without it
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define HS_API __attribute__((visibility("default")))

// a net's file: 'HSN1', side, outputs, then the floats (a stage's
// two convs, weights then biases, four stages; the two dense layers)
#define HS_MAGIC 0x314E5348
// a stage net with numbers joined before its dense layer ('HSA1',
// side, outputs, how many): the point model's box
#define HS_AUX_MAGIC 0x31415348
#define HS_STAGES 4

typedef struct {
    int    side, outputs;
    float* conv_w[HS_STAGES * 2];
    float* conv_b[HS_STAGES * 2];
    int    conv_in[HS_STAGES * 2], conv_out[HS_STAGES * 2];
    int    conv_k[HS_STAGES * 2];
    float *d1_w, *d1_b, *d2_w, *d2_b;
    int    d1_in, extra;
    float* data;
    // working planes: the widest stage, padded
    float *a, *b, *pad;
    // a find net (HSF1): its floats, read in order by hs_find_run
    int    is_find;
} hs_net;

// find's file: 'HSF1', side, 3, then the floats in the order
// export_eyes.py's find_floats writes them
#define HS_FIND_MAGIC 0x31465348

static const int hs_widths[HS_STAGES] = { 16, 32, 64, 128 };

HS_API void* hs_net_load(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    int head[4] = { 0 };
    if (fread(head, 4, 3, f) != 3
        || (head[0] != HS_MAGIC && head[0] != HS_FIND_MAGIC
            && head[0] != HS_AUX_MAGIC)
        || (head[0] == HS_AUX_MAGIC && fread(head + 3, 4, 1, f) != 1)) {
        fclose(f);
        return NULL;
    }
    hs_net* n = calloc(1, sizeof(hs_net));
    n->extra = head[3];
    n->side = head[1];
    n->outputs = head[2];
    n->is_find = head[0] == HS_FIND_MAGIC;
    long at = ftell(f);
    fseek(f, 0, SEEK_END);
    long count = (ftell(f) - at) / 4;
    fseek(f, at, SEEK_SET);
    n->data = malloc(count * 4);
    if (fread(n->data, 4, count, f) != (size_t)count) {
        fclose(f);
        free(n->data);
        free(n);
        return NULL;
    }
    fclose(f);
    if (n->is_find) return n;
    float* p = n->data;
    int last = n->side / 16;
    n->d1_in = 128 * last * last + n->extra;
    // every conv 3x3 but the first, its size from what is left over
    long rest = 128 * n->d1_in + 128 + n->outputs * 128 + n->outputs;
    int cin = 1;
    for (int s = 0; s < HS_STAGES; s++)
        for (int k = 0; k < 2; k++) {
            int cout = hs_widths[s];
            rest += (s + k ? cout * cin * 9 : 0) + cout;
            cin = cout;
        }
    long first = (count - rest) / hs_widths[0];
    int fk = 3;
    while (fk * fk < first) fk += 2;
    cin = 1;
    for (int s = 0; s < HS_STAGES; s++)
        for (int k = 0; k < 2; k++) {
            int i = s * 2 + k, cout = hs_widths[s];
            n->conv_in[i] = cin;
            n->conv_out[i] = cout;
            n->conv_k[i] = i ? 3 : fk;
            n->conv_w[i] = p;
            p += cout * cin * n->conv_k[i] * n->conv_k[i];
            n->conv_b[i] = p;
            p += cout;
            cin = cout;
        }
    n->d1_w = p;
    p += 128 * n->d1_in;
    n->d1_b = p;
    p += 128;
    n->d2_w = p;
    p += n->outputs * 128;
    n->d2_b = p;
    p += n->outputs;
    if (p - n->data != count) {
        free(n->data);
        free(n);
        return NULL;
    }
    int big = 16 * n->side * n->side;
    n->a = malloc(big * 4);
    n->b = malloc(big * 4);
    n->pad = malloc((n->side + fk) * (n->side + fk) * 4);
    return n;
}

// one conv (k x k, padding k / 2), then ReLU: inp [cin][h][h] to out
static void hs_conv(hs_net* n, int i, const float* inp, float* out,
                    int h)
{
    int cin = n->conv_in[i], cout = n->conv_out[i];
    int ks = n->conv_k[i], half = ks / 2, ph = h + 2 * half;
    const float* w = n->conv_w[i];
    for (int o = 0; o < cout; o++) {
        float* dst = out + o * h * h;
        for (int k = 0; k < h * h; k++) dst[k] = n->conv_b[i][o];
    }
    for (int c = 0; c < cin; c++) {
        // the plane padded with zeros once, read by every out channel
        memset(n->pad, 0, ph * ph * 4);
        for (int y = 0; y < h; y++)
            memcpy(n->pad + (y + half) * ph + half,
                   inp + c * h * h + y * h, h * 4);
        for (int o = 0; o < cout; o++) {
            float*       dst = out + o * h * h;
            const float* kw = w + (o * cin + c) * ks * ks;
            for (int ky = 0; ky < ks; ky++)
                for (int kx = 0; kx < ks; kx++) {
                    float wv = kw[ky * ks + kx];
                    for (int y = 0; y < h; y++) {
                        const float* src = n->pad + (y + ky) * ph + kx;
                        float*       d = dst + y * h;
                        for (int x = 0; x < h; x++) d[x] += wv * src[x];
                    }
                }
        }
    }
    for (int k = 0; k < cout * h * h; k++)
        if (out[k] < 0) out[k] = 0;
}

static void hs_pool(const float* inp, float* out, int c, int h)
{
    int g = h / 2;
    for (int k = 0; k < c; k++)
        for (int y = 0; y < g; y++)
            for (int x = 0; x < g; x++) {
                const float* s = inp + k * h * h + 2 * y * h + 2 * x;
                float m = s[0];
                if (s[1] > m) m = s[1];
                if (s[h] > m) m = s[h];
                if (s[h + 1] > m) m = s[h + 1];
                out[k * g * g + y * g + x] = m;
            }
}

// find's pieces. a plane set is [c][h][h]; conv k x k, stride st,
// padding k / 2, no bias unless b
static float* hs_fconv(const float* in, int cin, int h,
                       const float* w, const float* b, int cout,
                       int k, int st, int* ho)
{
    int half = k / 2, ph = h + 2 * half;
    int o = (h + 2 * half - k) / st + 1;
    float* out = calloc((size_t)cout * o * o, 4);
    float* pad = calloc((size_t)ph * ph, 4);
    for (int c = 0; c < cin; c++) {
        for (int y = 0; y < h; y++)
            memcpy(pad + (y + half) * ph + half, in + (c * h + y) * h,
                   h * 4);
        for (int q = 0; q < cout; q++) {
            float*       dst = out + q * o * o;
            const float* kw = w + ((size_t)q * cin + c) * k * k;
            for (int ky = 0; ky < k; ky++)
                for (int kx = 0; kx < k; kx++) {
                    float wv = kw[ky * k + kx];
                    for (int y = 0; y < o; y++) {
                        const float* src =
                            pad + (y * st + ky) * ph + kx;
                        float* d = dst + y * o;
                        if (st == 1)
                            for (int x = 0; x < o; x++)
                                d[x] += wv * src[x];
                        else
                            for (int x = 0; x < o; x++)
                                d[x] += wv * src[x * st];
                    }
                }
        }
    }
    if (b)
        for (int q = 0; q < cout; q++)
            for (int i = 0; i < o * o; i++)
                out[q * o * o + i] += b[q];
    free(pad);
    *ho = o;
    return out;
}

static float hs_silu(float v) { return v / (1.0f + expf(-v)); }

// GroupNorm(8): each group's mean and variance over its channels'
// planes, eps 1e-5, then each channel's scale and shift
static void hs_gnorm(float* x, int c, int h, const float* g,
                     const float* b, int silu)
{
    int per = c / 8, m = h * h;
    for (int gi = 0; gi < 8; gi++) {
        float* p = x + (size_t)gi * per * m;
        double sum = 0, sq = 0;
        for (int i = 0; i < per * m; i++) sum += p[i];
        double mean = sum / (per * m);
        for (int i = 0; i < per * m; i++)
            sq += (p[i] - mean) * (p[i] - mean);
        float inv = (float)(1.0 / sqrt(sq / (per * m) + 1e-5));
        for (int ch = 0; ch < per; ch++) {
            int   at = gi * per + ch;
            float* q = p + ch * m;
            for (int i = 0; i < m; i++) {
                float v = ((float)(q[i] - mean)) * inv * g[at] + b[at];
                q[i] = silu ? hs_silu(v) : v;
            }
        }
    }
}

// the reader over a find file's floats
typedef struct {
    const float* p;
} hs_take;

static const float* hs_next(hs_take* t, size_t n)
{
    const float* at = t->p;
    t->p += n;
    return at;
}

// a residual block: conv, norm, SiLU, conv, norm, plus the input
// (or its 1x1 conv), SiLU
static float* hs_block(hs_take* t, float* in, int cin, int h,
                       int cout, int st, int* ho)
{
    const float* w1 = hs_next(t, (size_t)cout * cin * 9);
    const float* g1 = hs_next(t, cout);
    const float* b1 = hs_next(t, cout);
    const float* w2 = hs_next(t, (size_t)cout * cout * 9);
    const float* g2 = hs_next(t, cout);
    const float* b2 = hs_next(t, cout);
    int    o, o2;
    float* a = hs_fconv(in, cin, h, w1, NULL, cout, 3, st, &o);
    hs_gnorm(a, cout, o, g1, b1, 1);
    float* c = hs_fconv(a, cout, o, w2, NULL, cout, 3, 1, &o2);
    free(a);
    hs_gnorm(c, cout, o, g2, b2, 0);
    if (cin == cout && st == 1) {
        for (size_t i = 0; i < (size_t)cout * o * o; i++)
            c[i] = hs_silu(c[i] + in[i]);
    } else {
        const float* ws = hs_next(t, (size_t)cout * cin);
        float* s = hs_fconv(in, cin, h, ws, NULL, cout, 1, st, &o2);
        for (size_t i = 0; i < (size_t)cout * o * o; i++)
            c[i] = hs_silu(c[i] + s[i]);
        free(s);
    }
    *ho = o;
    return c;
}

// bilinear to twice the size, as F.interpolate (align_corners off)
static float* hs_up2(const float* in, int c, int h)
{
    int    o = h * 2;
    float* out = malloc((size_t)c * o * o * 4);
    for (int y = 0; y < o; y++) {
        float sy = (y + 0.5f) / 2 - 0.5f;
        if (sy < 0) sy = 0;
        int   y0 = (int)sy, y1 = y0 + 1 < h ? y0 + 1 : h - 1;
        float fy = sy - y0;
        for (int x = 0; x < o; x++) {
            float sx = (x + 0.5f) / 2 - 0.5f;
            if (sx < 0) sx = 0;
            int   x0 = (int)sx, x1 = x0 + 1 < h ? x0 + 1 : h - 1;
            float fx = sx - x0;
            for (int k = 0; k < c; k++) {
                const float* p = in + (size_t)k * h * h;
                float top = (1 - fx) * p[y0 * h + x0]
                    + fx * p[y0 * h + x1];
                float low = (1 - fx) * p[y1 * h + x0]
                    + fx * p[y1 * h + x1];
                out[((size_t)k * o + y) * o + x] =
                    (1 - fy) * top + fy * low;
            }
        }
    }
    return out;
}

// find: x, y of the outer corners' midpoint and their distance, in
// shares of the picture (the heat map's softmax-weighted place)
static int hs_find_run(hs_net* n, const float* pic, float* out)
{
    hs_take t = { n->data };
    int     h = n->side, o;
    const float* sw = hs_next(&t, 32 * 25);
    const float* sg = hs_next(&t, 32);
    const float* sb = hs_next(&t, 32);
    float* stem = hs_fconv(pic, 1, h, sw, NULL, 32, 5, 1, &o);
    hs_gnorm(stem, 32, o, sg, sb, 1);
    float* full = hs_block(&t, stem, 32, o, 32, 1, &o);
    free(stem);
    int    hh, hq, he;
    float* x = hs_block(&t, full, 32, o, 48, 2, &hh);
    free(full);
    float* half = hs_block(&t, x, 48, hh, 48, 1, &hh);
    free(x);
    x = hs_block(&t, half, 48, hh, 96, 2, &hq);
    float* quarter = hs_block(&t, x, 96, hq, 96, 1, &hq);
    free(x);
    x = hs_block(&t, quarter, 96, hq, 160, 2, &he);
    float* eighth = hs_block(&t, x, 160, he, 160, 1, &he);
    free(x);
    const float* qw = hs_next(&t, 96 * 160);
    const float* qb = hs_next(&t, 96);
    x = hs_fconv(eighth, 160, he, qw, qb, 96, 1, 1, &o);
    float* up = hs_up2(x, 96, he);
    free(x);
    for (size_t i = 0; i < (size_t)96 * hq * hq; i++)
        up[i] += quarter[i];
    free(quarter);
    x = hs_block(&t, up, 96, hq, 96, 1, &o);
    free(up);
    const float* hw = hs_next(&t, 48 * 96);
    const float* hb = hs_next(&t, 48);
    float* y = hs_fconv(x, 96, hq, hw, hb, 48, 1, 1, &o);
    free(x);
    up = hs_up2(y, 48, hq);
    free(y);
    for (size_t i = 0; i < (size_t)48 * hh * hh; i++)
        up[i] += half[i];
    free(half);
    float* feat = hs_block(&t, up, 48, hh, 48, 1, &o);
    free(up);
    const float* tw = hs_next(&t, 48);
    const float* tb = hs_next(&t, 1);
    float  temp = expf(*hs_next(&t, 1));
    temp = temp < 1 ? 1 : temp > 100 ? 100 : temp;
    int    m = hh * hh;
    float* prob = malloc(m * 4);
    float  top = -1e30f;
    for (int i = 0; i < m; i++) {
        float v = tb[0];
        for (int c = 0; c < 48; c++)
            v += tw[c] * feat[(size_t)c * m + i];
        prob[i] = v * temp;
        if (prob[i] > top) top = prob[i];
    }
    double tot = 0;
    for (int i = 0; i < m; i++) tot += prob[i] = expf(prob[i] - top);
    float px = 0, py = 0, cat[208];
    for (int i = 0; i < m; i++) {
        prob[i] /= (float)tot;
        px += prob[i] * ((i % hh) + 0.5f) / hh;
        py += prob[i] * ((i / hh) + 0.5f) / hh;
    }
    for (int c = 0; c < 160; c++) {
        float v = 0;
        for (int i = 0; i < he * he; i++)
            v += eighth[(size_t)c * he * he + i];
        cat[c] = v / (he * he);
    }
    for (int c = 0; c < 48; c++) {
        float v = 0;
        for (int i = 0; i < m; i++)
            v += feat[(size_t)c * m + i] * prob[i];
        cat[160 + c] = v;
    }
    free(prob);
    free(feat);
    free(eighth);
    const float* g1 = hs_next(&t, 96 * 208);
    const float* gb = hs_next(&t, 96);
    const float* g2 = hs_next(&t, 96);
    const float* g2b = hs_next(&t, 1);
    float hid[96], gap = g2b[0];
    for (int q = 0; q < 96; q++) {
        float v = gb[q];
        for (int c = 0; c < 208; c++) v += g1[q * 208 + c] * cat[c];
        hid[q] = hs_silu(v);
    }
    for (int q = 0; q < 96; q++) gap += g2[q] * hid[q];
    out[0] = px;
    out[1] = py;
    out[2] = 1.0f / (1.0f + expf(-gap));
    return 3;
}

static int hs_net_run_aux(void* net, const float* img,
                          const float* aux, float* out);

// a side x side picture (0..1) in, the net's outputs out; the
// picture made mean 0, deviation 1 first, as training did
HS_API int hs_net_run(void* net, const float* img, float* out)
{
    return hs_net_run_aux(net, img, NULL, out);
}

// the same with its extra numbers (aux: n->extra of them) joined to
// the flattened stages
static int hs_net_run_aux(void* net, const float* img,
                          const float* aux, float* out)
{
    hs_net* n = net;
    if (!n) return 0;
    int   h = n->side, m = h * h;
    double sum = 0, sq = 0;
    for (int k = 0; k < m; k++) sum += img[k];
    double mean = sum / m;
    for (int k = 0; k < m; k++) sq += (img[k] - mean) * (img[k] - mean);
    double dev = sqrt(sq / (m - 1));
    if (dev < 1e-4) dev = 1e-4;
    if (n->is_find) {
        float* pic = malloc(m * 4);
        for (int k = 0; k < m; k++)
            pic[k] = (float)((img[k] - mean) / dev);
        int r = hs_find_run(n, pic, out);
        free(pic);
        return r;
    }
    for (int k = 0; k < m; k++)
        n->a[k] = (float)((img[k] - mean) / dev);
    for (int s = 0; s < HS_STAGES; s++) {
        hs_conv(n, s * 2, n->a, n->b, h);
        hs_conv(n, s * 2 + 1, n->b, n->a, h);
        hs_pool(n->a, n->b, hs_widths[s], h);
        h /= 2;
        memcpy(n->a, n->b, hs_widths[s] * h * h * 4);
    }
    float hid[128];
    for (int o = 0; o < 128; o++) {
        float        v = n->d1_b[o];
        const float* w = n->d1_w + o * n->d1_in;
        int          flat = n->d1_in - n->extra;
        for (int k = 0; k < flat; k++) v += w[k] * n->a[k];
        for (int e = 0; e < n->extra; e++)
            v += w[flat + e] * (aux ? aux[e] : 0);
        hid[o] = v > 0 ? v : 0;
    }
    for (int o = 0; o < n->outputs; o++) {
        float        v = n->d2_b[o];
        const float* w = n->d2_w + o * 128;
        for (int k = 0; k < 128; k++) v += w[k] * hid[k];
        out[o] = v;
    }
    return n->outputs;
}

// a picture's crop as grid_sample takes it (bilinear, zeros past the
// edge, align_corners off): out side x side, its x axis (ux, uy)
// and y axis (vx, vy) in shares of the frame, centred at (cx, cy)
static void hs_crop(const float* img, int w, int h, float cx, float cy,
                    float ux, float uy, float vx, float vy, int side,
                    int mirror, float* out)
{
    for (int i = 0; i < side; i++)
        for (int j = 0; j < side; j++) {
            float gx = (2.0f * j + 1) / side - 1;
            float gy = (2.0f * i + 1) / side - 1;
            float nx = ux * gx + vx * gy + cx * 2 - 1;
            float ny = uy * gx + vy * gy + cy * 2 - 1;
            float px = ((nx + 1) * w - 1) / 2;
            float py = ((ny + 1) * h - 1) / 2;
            int   x0 = (int)floorf(px), y0 = (int)floorf(py);
            float fx = px - x0, fy = py - y0, v = 0;
            for (int dy = 0; dy < 2; dy++)
                for (int dx = 0; dx < 2; dx++) {
                    int x = x0 + dx, y = y0 + dy;
                    if (x < 0 || y < 0 || x >= w || y >= h) continue;
                    float wt = (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy);
                    v += wt * img[y * w + x];
                }
            out[i * side + (mirror ? side - 1 - j : j)] = v;
        }
}

// the frame shrunk to side x side by area (adaptive average pooling)
static void hs_area(const float* img, int w, int h, int side,
                    float* out)
{
    for (int i = 0; i < side; i++) {
        int y0 = i * h / side, y1 = ((i + 1) * h + side - 1) / side;
        for (int j = 0; j < side; j++) {
            int x0 = j * w / side, x1 = ((j + 1) * w + side - 1) / side;
            double s = 0;
            for (int y = y0; y < y1; y++)
                for (int x = x0; x < x1; x++) s += img[y * w + x];
            out[i * side + j] = (float)(s / ((y1 - y0) * (x1 - x0)));
        }
    }
}

// the eye stage's crop width, in outer corner to outer corner
#define HS_EYE_SPAN 0.7f
// target's square: this many outer-corner gaps across
#define HS_SPAN 2.3f

// a grey frame (w x h bytes) through find, target and the eye stage:
// out[0..2] find's eye middle and square (frame shares), out[3..10]
// the outer corners then pupils (left, right as the picture has
// them) in frame pixels. eye may be NULL: target's points alone
// the eye stage on each eye of q (the four points, frame shares),
// its answer put back in q; pic a buffer of the eye net's side
static void hs_eyes(hs_net* en, const float* img, int w, int h,
                    float* q, float* pic)
{
    float r[16];
    float line = atan2f(q[3] - q[1], q[2] - q[0]);
    float sc = hypotf(q[2] - q[0], q[3] - q[1]);
    if (sc < 1e-3f) sc = 1e-3f;
    float es = sc * HS_EYE_SPAN, c = cosf(line), sn = sinf(line);
    float p[8];
    memcpy(p, q, sizeof(p));
    for (int e = 0; e < 2; e++) {
        float ex = (p[2 * e] + p[4 + 2 * e]) / 2;
        float ey = (p[2 * e + 1] + p[5 + 2 * e]) / 2;
        hs_crop(img, w, h, ex, ey, es * c, es * sn, -es * sn,
                es * c, en->side, e, pic);
        hs_net_run(en, pic, r);
        for (int j = 0; j < 4; j += 2) {
            float u = e ? 1 - r[j] : r[j];
            float du = (u - 0.5f) * es, dv = (r[j + 1] - 0.5f) * es;
            int   slot = j == 0 ? 2 * e : 4 + 2 * e;
            q[slot] = ex + c * du - sn * dv;
            q[slot + 1] = ey + sn * du + c * dv;
        }
    }
}

// one camera's answer judged alone, as train_gaze.py's judge: 1
// sound, 0 not (a wrong shape, or the eye stage rerun on its own
// answer moving a point over 6% of the corner gap). p in pixels
static int hs_sound(const float* p, const float* again, int w)
{
    float dx = p[2] - p[0], dy = p[3] - p[1];
    float sc = hypotf(dx, dy);
    if (sc < 0.06f * w || sc > 0.6f * w) return 0;
    float ux = dx / sc, uy = dy / sc, nx = -uy, ny = ux;
    float la = ((p[4] - p[0]) * ux + (p[5] - p[1]) * uy) / sc;
    float ra = ((p[6] - p[2]) * ux + (p[7] - p[3]) * uy) / sc;
    float lb = ((p[4] - p[0]) * nx + (p[5] - p[1]) * ny) / sc;
    float rb = ((p[6] - p[2]) * nx + (p[7] - p[3]) * ny) / sc;
    if (!(la > 0.08f && la < 0.45f && ra > -0.45f && ra < -0.08f))
        return 0;
    if (fabsf(lb) > 0.15f || fabsf(rb) > 0.15f || fabsf(uy) > 0.64f)
        return 0;
    for (int k = 0; k < 8; k++)
        if (fabsf(again[k] - p[k]) > 0.06f * sc) return 0;
    return 1;
}

// the gaze clips and numbers of one camera, as train_gaze.py's
// camera_clips cuts them: left eye, right eye mirrored, the upper
// face (HS_CLIP square each), then 18 numbers. p in pixels
#define HS_CLIP 32
#define HS_GAZE_EYE_SPAN (1.25f / 3)
#define HS_GAZE_EYE_IN 0.17f
#define HS_GAZE_FACE_SPAN 1.15f
#define HS_GAZE_FACE_DROP 0.1f
#define HS_NAUX 18

static void hs_gaze_cut(const float* img, int w, int h, const float* p,
                        float* clips, float* aux)
{
    float dx = p[2] - p[0], dy = p[3] - p[1];
    float sc = hypotf(dx, dy);
    if (sc < 1) sc = 1;
    float ang = atan2f(dy, dx), c = cosf(ang), s = sinf(ang);
    float cx[3], cy[3], side[3];
    cx[0] = p[0] + dx * HS_GAZE_EYE_IN;
    cy[0] = p[1] + dy * HS_GAZE_EYE_IN;
    cx[1] = p[2] - dx * HS_GAZE_EYE_IN;
    cy[1] = p[3] - dy * HS_GAZE_EYE_IN;
    side[0] = side[1] = sc * HS_GAZE_EYE_SPAN;
    cx[2] = (p[0] + p[2]) / 2 - dy * HS_GAZE_FACE_DROP;
    cy[2] = (p[1] + p[3]) / 2 + dx * HS_GAZE_FACE_DROP;
    side[2] = sc * HS_GAZE_FACE_SPAN;
    for (int k = 0; k < 3; k++) {
        float d = side[k];
        hs_crop(img, w, h, cx[k] / w, cy[k] / h, d * c / w, d * s / h,
                -d * s / w, d * c / h, HS_CLIP, k == 1,
                clips + k * HS_CLIP * HS_CLIP);
    }
    // pupils then outer corners, centred; the gap; each clip's box
    int order[4] = { 4, 6, 0, 2 };
    for (int k = 0; k < 4; k++) {
        aux[2 * k] = p[order[k]] / w - 0.5f;
        aux[2 * k + 1] = p[order[k] + 1] / h - 0.5f;
    }
    aux[8] = sc / w;
    for (int k = 0; k < 3; k++) {
        aux[9 + 3 * k] = cx[k] / w - 0.5f;
        aux[10 + 3 * k] = cy[k] / h - 0.5f;
        aux[11 + 3 * k] = side[k] / w;
    }
}

HS_API int hs_run(void* find, void* target, void* eye,
                  const unsigned char* grey, int w, int h, float* out)
{
    hs_net *fn = find, *tn = target, *en = eye;
    if (!fn || !tn) return 0;
    float* img = malloc(w * h * 4);
    for (int k = 0; k < w * h; k++) img[k] = grey[k] / 255.0f;
    int    side = fn->side;
    float* pic = malloc(64 * 64 * 4 > side * side * 4
                            ? 64 * 64 * 4 : side * side * 4);
    float  r[16];
    hs_area(img, w, h, side, pic);
    hs_net_run(fn, pic, r);
    // find: the outer corners' midpoint and their distance
    float cx = r[0], cy = r[1], s = r[2] * HS_SPAN;
    out[0] = cx;
    out[1] = cy;
    out[2] = s;
    side = tn->side;
    hs_crop(img, w, h, cx, cy, s, 0, 0, s, side, 0, pic);
    hs_net_run(tn, pic, r);
    float q[8];
    for (int k = 0; k < 8; k++)
        q[k] = (k % 2 == 0) ? cx + (r[k] - 0.5f) * s
                            : cy + (r[k] - 0.5f) * s;
    if (en) hs_eyes(en, img, w, h, q, pic);
    for (int k = 0; k < 8; k++) out[3 + k] = q[k] * (k % 2 ? h : w);
    free(pic);
    free(img);
    return 1;
}

// a camera's own thread: the newest frame handed in, the chain run
// on it, the answer kept for the app's next look; the app never
// waits on the nets
#include <pthread.h>

typedef struct {
    void *find, *target, *eye;
    pthread_t       thread;
    pthread_mutex_t lock;
    pthread_cond_t  wake;
    unsigned char*  frame;
    int             w, h, fresh, quit;
    // the frame the last answer came from
    unsigned char*  shown;
    int             shown_cap;
    float           out[11];
    int             done, gdone;
    float           ms;
    // for gaze: this answer's clips, numbers and its check
    float           clips[3 * HS_CLIP * HS_CLIP], aux[HS_NAUX];
    int             sound;
} hs_worker;

#include <time.h>

static void* hs_work(void* arg)
{
    hs_worker*     k = arg;
    unsigned char* mine = NULL;
    int            cap = 0;
    for (;;) {
        pthread_mutex_lock(&k->lock);
        while (!k->fresh && !k->quit)
            pthread_cond_wait(&k->wake, &k->lock);
        if (k->quit) {
            pthread_mutex_unlock(&k->lock);
            break;
        }
        int w = k->w, h = k->h;
        if (cap < w * h) {
            free(mine);
            cap = w * h;
            mine = malloc(cap);
        }
        memcpy(mine, k->frame, w * h);
        k->fresh = 0;
        pthread_mutex_unlock(&k->lock);
        float           o[11];
        struct timespec a, b;
        clock_gettime(CLOCK_MONOTONIC, &a);
        int ok = hs_run(k->find, k->target, k->eye, mine, w, h, o);
        clock_gettime(CLOCK_MONOTONIC, &b);
        // the points first: the app draws them before the gaze work
        pthread_mutex_lock(&k->lock);
        if (ok) {
            if (k->shown_cap < w * h) {
                free(k->shown);
                k->shown = malloc(w * h);
                k->shown_cap = w * h;
            }
            memcpy(k->shown, mine, w * h);
            memcpy(k->out, o, sizeof(o));
            k->done++;
            k->ms = (b.tv_sec - a.tv_sec) * 1e3f
                  + (b.tv_nsec - a.tv_nsec) / 1e6f;
        }
        pthread_mutex_unlock(&k->lock);
        // the gaze clips, the check (the eye stage rerun on its
        // own answer)
        float clips[3 * HS_CLIP * HS_CLIP], aux[HS_NAUX];
        int   sound = 0;
        if (ok && k->eye) {
            hs_net* en = k->eye;
            float*  img = malloc(w * h * 4);
            float*  pic = malloc(en->side * en->side * 4);
            for (int i = 0; i < w * h; i++) img[i] = mine[i] / 255.0f;
            float q[8], again[8];
            for (int i = 0; i < 8; i++)
                q[i] = o[3 + i] / (i % 2 ? h : w);
            hs_eyes(en, img, w, h, q, pic);
            for (int i = 0; i < 8; i++)
                again[i] = q[i] * (i % 2 ? h : w);
            sound = hs_sound(o + 3, again, w);
            hs_gaze_cut(img, w, h, o + 3, clips, aux);
            free(pic);
            free(img);
        }
        pthread_mutex_lock(&k->lock);
        if (ok) {
            memcpy(k->clips, clips, sizeof(clips));
            memcpy(k->aux, aux, sizeof(aux));
            k->sound = sound;
            k->gdone++;
        }
        pthread_mutex_unlock(&k->lock);
    }
    free(mine);
    return NULL;
}

HS_API void hs_net_free(void* net)
{
    hs_net* n = net;
    if (!n) return;
    free(n->data);
    free(n->a);
    free(n->b);
    free(n->pad);
    free(n);
}

// a worker with its own nets from a folder (find.hsn, target.hsn,
// eye.hsn; the eye stage may be missing); NULL without find and target
HS_API void* hs_worker_open(const char* dir)
{
    char  p[1024];
    void* nets[3];
    const char* names[3] = { "find", "target", "eye" };
    for (int i = 0; i < 3; i++) {
        snprintf(p, sizeof(p), "%s/%s.hsn", dir, names[i]);
        nets[i] = hs_net_load(p);
    }
    if (!nets[0] || !nets[1]) {
        for (int i = 0; i < 3; i++) hs_net_free(nets[i]);
        return NULL;
    }
    hs_worker* k = calloc(1, sizeof(hs_worker));
    k->find = nets[0];
    k->target = nets[1];
    k->eye = nets[2];
    pthread_mutex_init(&k->lock, NULL);
    pthread_cond_init(&k->wake, NULL);
    pthread_create(&k->thread, NULL, hs_work, k);
    return k;
}

// the newest grey frame; one not yet taken is replaced
HS_API void hs_worker_put(void* worker, const unsigned char* grey,
                          int w, int h)
{
    hs_worker* k = worker;
    if (!k || !grey) return;
    pthread_mutex_lock(&k->lock);
    if (k->w * k->h != w * h) {
        free(k->frame);
        k->frame = malloc(w * h);
    }
    k->w = w;
    k->h = h;
    memcpy(k->frame, grey, w * h);
    k->fresh = 1;
    pthread_cond_signal(&k->wake);
    pthread_mutex_unlock(&k->lock);
}

// the last answer (hs_run's 11 numbers) and the frame it came from
// (grey: w x h bytes, or NULL); how many frames it has answered so
// far, 0 none yet. ms: the last frame's time
HS_API int hs_worker_get(void* worker, float* out, float* ms,
                         unsigned char* grey, int w, int h)
{
    hs_worker* k = worker;
    if (!k) return 0;
    pthread_mutex_lock(&k->lock);
    memcpy(out, k->out, sizeof(k->out));
    if (grey && k->shown && k->shown_cap >= w * h)
        memcpy(grey, k->shown, w * h);
    int n = k->done;
    if (ms) *ms = k->ms;
    pthread_mutex_unlock(&k->lock);
    return n;
}

// the last answer's gaze clips and numbers, and its check (1
// sound); how many frames answered so far
HS_API int hs_worker_gaze(void* worker, float* clips, float* aux,
                          int* sound)
{
    hs_worker* k = worker;
    if (!k) return 0;
    pthread_mutex_lock(&k->lock);
    memcpy(clips, k->clips, sizeof(k->clips));
    memcpy(aux, k->aux, sizeof(k->aux));
    *sound = k->sound;
    int n = k->gdone;
    pthread_mutex_unlock(&k->lock);
    return n;
}

// the gaze net (train_gaze.py's GazeNet): 'HSG4', the encoders'
// first conv kernel, the cameras it reads (1 or 2) and the first of
// them; then for the eye encoder and the face encoder six convs
// (batch norm folded) and their dense layer; the head's layer norm
// and two dense layers. Held as those three numbers, then the floats
#define HS_GAZE_MAGIC 0x34475348
#define HS_GAZE_IN (6 * 64 + 2 * HS_NAUX + 2)

HS_API void* hs_gaze_load(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    int head[4];
    if (fread(head, 4, 4, f) != 4 || head[0] != HS_GAZE_MAGIC
        || head[2] < 1 || head[2] > 2 || head[3] < 0
        || head[3] + head[2] > 2) {
        fclose(f);
        return NULL;
    }
    long at = ftell(f);
    fseek(f, 0, SEEK_END);
    long count = (ftell(f) - at) / 4;
    fseek(f, at, SEEK_SET);
    float* data = malloc((count + 3) * 4);
    for (int i = 0; i < 3; i++) data[i] = (float)head[1 + i];
    if (fread(data + 3, 4, count, f) != (size_t)count) {
        free(data);
        data = NULL;
    }
    fclose(f);
    return data;
}

// whether the gaze net reads camera c (0 or 1)
HS_API int hs_gaze_uses(void* net, int c)
{
    const float* nf = net;
    if (!nf) return 0;
    int n = (int)nf[1], first = (int)nf[2];
    return c >= first && c < first + n;
}

HS_API void hs_gaze_free(void* net) { free(net); }

// one encoder on one clip: three stages of two 3x3 convs and ReLU,
// a 2x2 max pool each, then the dense layer to 64 numbers
static void hs_gaze_enc(hs_take* t, const float* clip, int fk,
                        float* out)
{
    int    m = HS_CLIP * HS_CLIP, h = HS_CLIP, o;
    double sum = 0, sq = 0;
    for (int i = 0; i < m; i++) sum += clip[i];
    double mean = sum / m;
    for (int i = 0; i < m; i++)
        sq += (clip[i] - mean) * (clip[i] - mean);
    double dev = sqrt(sq / (m - 1));
    if (dev < 1e-4) dev = 1e-4;
    float* x = malloc(m * 4);
    for (int i = 0; i < m; i++) x[i] = (float)((clip[i] - mean) / dev);
    int widths[3] = { 16, 32, 64 }, cin = 1;
    for (int s = 0; s < 3; s++) {
        for (int k = 0; k < 2; k++) {
            int          cout = widths[s];
            int          ks = (s + k) ? 3 : fk;
            const float* w = hs_next(t, (size_t)cout * cin * ks * ks);
            const float* b = hs_next(t, cout);
            float*       y = hs_fconv(x, cin, h, w, b, cout, ks, 1, &o);
            for (size_t i = 0; i < (size_t)cout * h * h; i++)
                if (y[i] < 0) y[i] = 0;
            free(x);
            x = y;
            cin = cout;
        }
        float* y = malloc((size_t)cin * (h / 2) * (h / 2) * 4);
        hs_pool(x, y, cin, h);
        free(x);
        x = y;
        h /= 2;
    }
    int          n = cin * h * h;
    const float* w = hs_next(t, (size_t)64 * n);
    const float* b = hs_next(t, 64);
    for (int q = 0; q < 64; q++) {
        float v = b[q];
        for (int i = 0; i < n; i++) v += w[q * n + i] * x[i];
        out[q] = v;
    }
    free(x);
}

// the dot from both cameras: clips (3 each), numbers (18 each) and
// checks; out x, y on the screen, -0.5 to 0.5 from its middle
HS_API int hs_gaze_run(void* net, const float* clips0,
                       const float* aux0, int sound0,
                       const float* clips1, const float* aux1,
                       int sound1, float* out)
{
    if (!net) return 0;
    float  in[HS_GAZE_IN];
    int    cc = HS_CLIP * HS_CLIP;
    const float* cl[2] = { clips0, clips1 };
    const float* ax[2] = { aux0, aux1 };
    int          sd[2] = { sound0, sound1 };
    const float* nf = net;
    int fk = (int)nf[0], n = (int)nf[1], first = (int)nf[2];
    int wide = 3 * n * 64 + n * HS_NAUX + n;
    hs_take t = { nf + 3 };
    // the eye encoder on each camera's two eyes, then the face one
    // on each face: each reads its weights again from its start
    hs_take eyes = t, faces;
    for (int k = 0; k < 2 * n; k++) {
        eyes = t;
        hs_gaze_enc(&eyes, cl[first + k / 2] + (k % 2) * cc, fk,
                    in + 64 * k);
    }
    for (int k = 0; k < n; k++) {
        faces = eyes;
        hs_gaze_enc(&faces, cl[first + k] + 2 * cc, fk,
                    in + 128 * n + 64 * k);
    }
    t = faces;
    for (int k = 0; k < n; k++) {
        memcpy(in + 192 * n + HS_NAUX * k, ax[first + k],
               HS_NAUX * 4);
        in[192 * n + HS_NAUX * n + k] = (float)sd[first + k];
    }
    // layer norm, eps 1e-5
    const float* lg = hs_next(&t, wide);
    const float* lb = hs_next(&t, wide);
    double mean = 0, var = 0;
    for (int i = 0; i < wide; i++) mean += in[i];
    mean /= wide;
    for (int i = 0; i < wide; i++)
        var += (in[i] - mean) * (in[i] - mean);
    float inv = (float)(1.0 / sqrt(var / wide + 1e-5));
    for (int i = 0; i < wide; i++)
        in[i] = (float)(in[i] - mean) * inv * lg[i] + lb[i];
    const float* w1 = hs_next(&t, 256 * wide);
    const float* b1 = hs_next(&t, 256);
    const float* w2 = hs_next(&t, 2 * 256);
    const float* b2 = hs_next(&t, 2);
    float hid[256];
    for (int q = 0; q < 256; q++) {
        float v = b1[q];
        for (int i = 0; i < wide; i++)
            v += w1[q * wide + i] * in[i];
        hid[q] = v > 0 ? v : 0;
    }
    for (int q = 0; q < 2; q++) {
        float v = b2[q];
        for (int i = 0; i < 256; i++) v += w2[q * 256 + i] * hid[i];
        out[q] = v;
    }
    return 2;
}

HS_API void hs_worker_free(void* worker)
{
    hs_worker* k = worker;
    if (!k) return;
    pthread_mutex_lock(&k->lock);
    k->quit = 1;
    pthread_cond_signal(&k->wake);
    pthread_mutex_unlock(&k->lock);
    pthread_join(k->thread, NULL);
    hs_net_free(k->find);
    hs_net_free(k->target);
    hs_net_free(k->eye);
    free(k->frame);
    free(k->shown);
    free(k);
}

// the hand nets (ai/HyperHands.ag): handfind.hsn, a hand and its
// box, on the whole frame; hands.hsn, each finger's bend and the
// spread, and point.hsn, the point on the screen the hand points
// at, both on the finder's crop, as training's held-back pictures
// a net's picture kept a quarter larger than its side, then made
// its side (ai/HyperHands.ag hh_keep)
#define HS_HAND_KEEP(side) ((side) * 5 / 4)
#define HS_HAND_NOUT 14

typedef struct {
    void *hands, *point, *find;
    pthread_t       thread;
    pthread_mutex_t lock;
    pthread_cond_t  wake;
    unsigned char*  frame;
    int             w, h, fresh, quit;
    float           out[HS_HAND_NOUT];
    int             done;
    float           ms;
} hs_hand;

// the kept picture (HS_HAND_KEEP across) to side x side, bilinear
// over the whole of it, as training's held-back pictures
static void hs_hand_side(const float* keep, int side, float* pic)
{
    int   kp = HS_HAND_KEEP(side);
    float step = (float)kp / side;
    for (int py = 0; py < side; py++) {
        float sy = (py + 0.5f) * step - 0.5f;
        int   y0 = (int)floorf(sy);
        if (y0 > kp - 2) y0 = kp - 2;
        if (y0 < 0) y0 = 0;
        float fy = fminf(fmaxf(sy - y0, 0), 1);
        for (int px = 0; px < side; px++) {
            float sx = (px + 0.5f) * step - 0.5f;
            int   x0 = (int)floorf(sx);
            if (x0 > kp - 2) x0 = kp - 2;
            if (x0 < 0) x0 = 0;
            float fx = fminf(fmaxf(sx - x0, 0), 1);
            const float* a = keep + y0 * kp + x0;
            float top = a[0] * (1 - fx) + a[1] * fx;
            float low = a[kp] * (1 - fx) + a[kp + 1] * fx;
            pic[py * side + px] = top * (1 - fy) + low * fy;
        }
    }
}

// the whole frame kept by area, then to side
static void hs_hand_pic(const unsigned char* grey, int w, int h,
                        int side, float* keep, float* pic)
{
    float* img = malloc(w * h * 4);
    for (int k = 0; k < w * h; k++) img[k] = grey[k] / 255.0f;
    hs_area(img, w, h, HS_HAND_KEEP(side), keep);
    free(img);
    hs_hand_side(keep, side, pic);
}

// the finder's crop: a square about (cx, cy) (shares of the
// frame), sd of its width across, kp across; each pixel the mean of
// a grid of bilinear reads covering it, the edges held, rounded to
// grey levels as training's (ai/HyperHands.ag hp_crop)
// its side: the box made this much larger (HP_CROP)
#define HS_POINT_CROP 1.25f
static double hs_grey_at(const unsigned char* g, int w, int h,
                         double sx, double sy)
{
    sx = fmin(fmax(sx, 0), w - 1.001);
    sy = fmin(fmax(sy, 0), h - 1.001);
    int    x0 = (int)floor(sx), y0 = (int)floor(sy);
    double fx = sx - x0, fy = sy - y0;
    const unsigned char* a = g + y0 * w + x0;
    double top = a[0] * (1 - fx) + a[1] * fx;
    double low = a[w] * (1 - fx) + a[w + 1] * fx;
    return top * (1 - fy) + low * fy;
}

static void hs_hand_crop(const unsigned char* g, int w, int h,
                         float cx, float cy, float sd, int kp,
                         float* keep)
{
    double span = (double)sd * w, px = span / kp;
    int    n = (int)ceil(px);
    if (n < 1) n = 1;
    if (n > 8) n = 8;
    for (int i = 0; i < kp; i++)
        for (int j = 0; j < kp; j++) {
            double sum = 0;
            for (int a = 0; a < n; a++)
                for (int b = 0; b < n; b++) {
                    double sy = cy * (double)h - span / 2
                              + (i + (a + 0.5) / n) * px - 0.5;
                    double sx = cx * (double)w - span / 2
                              + (j + (b + 0.5) / n) * px - 0.5;
                    sum += hs_grey_at(g, w, h, sx, sy);
                }
            double v = floor(sum / (n * n) + 0.5);
            keep[i * kp + j] = (float)(fmin(fmax(v, 0), 255) / 255.0);
        }
}

static void* hs_hand_work(void* arg)
{
    hs_hand*       k = arg;
    unsigned char* mine = NULL;
    int            cap = 0;
    // room for a net up to 256 across
    float*         keep = malloc(320 * 320 * 4);
    for (;;) {
        pthread_mutex_lock(&k->lock);
        while (!k->fresh && !k->quit)
            pthread_cond_wait(&k->wake, &k->lock);
        if (k->quit) {
            pthread_mutex_unlock(&k->lock);
            break;
        }
        int w = k->w, h = k->h;
        if (cap < w * h) {
            free(mine);
            cap = w * h;
            mine = malloc(cap);
        }
        memcpy(mine, k->frame, w * h);
        k->fresh = 0;
        pthread_mutex_unlock(&k->lock);
        struct timespec a, b;
        clock_gettime(CLOCK_MONOTONIC, &a);
        float o[HS_HAND_NOUT] = { 0 }, r[16];
        // the finder on the whole frame
        hs_net* fn = k->find;
        if (fn && fn->outputs == 4 && fn->side <= 256) {
            float* pic = malloc(fn->side * fn->side * 4);
            hs_hand_pic(mine, w, h, fn->side, keep, pic);
            hs_net_run(fn, pic, o + 10);
            free(pic);
        }
        // a hand found: the gesture and point models on its crop
        // (the box made HS_POINT_CROP larger), point with the box
        float box[3] = { o[11] - 0.5f, o[12] - 0.5f, o[13] };
        o[0] = o[10];
        int found = fn && o[10] > 0.5f && o[13] >= 0.02f;
        hs_net* gn = k->hands;
        hs_net* pn = k->point;
        void* nets[2] = { gn, pn };
        for (int i = 0; i < 2 && found; i++) {
            hs_net* n = nets[i];
            if (!n || n->side > 256) continue;
            if (i == 0 && (n->outputs != 6 || n->extra)) continue;
            if (i == 1 && (n->outputs != 2 || n->extra != 3)) continue;
            float* pic = malloc(n->side * n->side * 4);
            hs_hand_crop(mine, w, h, o[11], o[12],
                         o[13] * HS_POINT_CROP,
                         HS_HAND_KEEP(n->side), keep);
            hs_hand_side(keep, n->side, pic);
            hs_net_run_aux(n, pic, box, r);
            if (i == 0) memcpy(o + 1, r, 6 * 4);
            else {
                o[7] = 1;
                o[8] = r[0];
                o[9] = r[1];
            }
            free(pic);
        }
        clock_gettime(CLOCK_MONOTONIC, &b);
        pthread_mutex_lock(&k->lock);
        memcpy(k->out, o, sizeof(o));
        k->done++;
        k->ms = (b.tv_sec - a.tv_sec) * 1e3f
              + (b.tv_nsec - a.tv_nsec) / 1e6f;
        pthread_mutex_unlock(&k->lock);
    }
    free(keep);
    free(mine);
    return NULL;
}

// both hand nets from a folder (models/hands); NULL with neither
HS_API void* hs_hand_open(const char* dir)
{
    char p[1024];
    snprintf(p, sizeof(p), "%s/hands.hsn", dir);
    void* hands = hs_net_load(p);
    snprintf(p, sizeof(p), "%s/point.hsn", dir);
    void* point = hs_net_load(p);
    snprintf(p, sizeof(p), "%s/handfind.hsn", dir);
    void* find = hs_net_load(p);
    if (!hands && !point && !find) return NULL;
    hs_hand* k = calloc(1, sizeof(hs_hand));
    k->hands = hands;
    k->point = point;
    k->find = find;
    pthread_mutex_init(&k->lock, NULL);
    pthread_cond_init(&k->wake, NULL);
    pthread_create(&k->thread, NULL, hs_hand_work, k);
    return k;
}

// which nets loaded: 1 hands, 2 point, 4 the finder
HS_API int hs_hand_has(void* hand)
{
    hs_hand* k = hand;
    if (!k) return 0;
    return (k->hands ? 1 : 0) | (k->point ? 2 : 0) | (k->find ? 4 : 0);
}

HS_API void hs_hand_put(void* hand, const unsigned char* grey, int w,
                        int h)
{
    hs_hand* k = hand;
    if (!k || !grey) return;
    pthread_mutex_lock(&k->lock);
    if (k->w * k->h != w * h) {
        free(k->frame);
        k->frame = malloc(w * h);
    }
    k->w = w;
    k->h = h;
    memcpy(k->frame, grey, w * h);
    k->fresh = 1;
    pthread_cond_signal(&k->wake);
    pthread_mutex_unlock(&k->lock);
}

// the last answer: hand (the finder's), thumb, index, middle, ring,
// little, spread (the gesture model's, on the finder's crop);
// the point (1 when read; x, y from the screen's middle, shares of
// its width and height), read from the finder's crop and box; the
// finder's hand and box
// (middle x, y and side, shares of the frame); frames answered
HS_API int hs_hand_get(void* hand, float* out, float* ms)
{
    hs_hand* k = hand;
    if (!k) return 0;
    pthread_mutex_lock(&k->lock);
    memcpy(out, k->out, sizeof(k->out));
    int n = k->done;
    if (ms) *ms = k->ms;
    pthread_mutex_unlock(&k->lock);
    return n;
}

HS_API void hs_hand_free(void* hand)
{
    hs_hand* k = hand;
    if (!k) return;
    pthread_mutex_lock(&k->lock);
    k->quit = 1;
    pthread_cond_signal(&k->wake);
    pthread_mutex_unlock(&k->lock);
    pthread_join(k->thread, NULL);
    hs_net_free(k->hands);
    hs_net_free(k->point);
    hs_net_free(k->find);
    free(k->frame);
    free(k);
}
