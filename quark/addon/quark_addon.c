// quark's Node addon: trinity's canvas, fonts, paths and images, as
// webgfx exports them in C, plus the frame share and an fd send
#define NAPI_VERSION 8
#include <node_api.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// webgfx (libsilver-webgfx.so)
void  webgfx_gpu_init(void);
int   webgfx_font_open(uint8_t* data, int64_t size, int index);
void  webgfx_font_close(int id);
int   webgfx_font_glyph_count(int id);
int   webgfx_font_units_per_em(int id);
uint32_t webgfx_font_glyph_index(int id, uint32_t cp);
void  webgfx_font_metrics(int id, float px, float* out);
int   webgfx_font_family(int id, uint8_t* buf, int cap);
float webgfx_font_advance(int id, uint32_t glyph, float px);
bool  webgfx_font_match(const char* family, int weight, int width, int slope,
    uint32_t cp, bool emoji, uint8_t* file, int file_cap, uint8_t* fam, int fam_cap, int* index);
int   webgfx_path_new(void);
void  webgfx_path_free(int id);
void  webgfx_path_move_to(int id, float x, float y);
void  webgfx_path_line_to(int id, float x, float y);
void  webgfx_path_quad_to(int id, float cx, float cy, float x, float y);
void  webgfx_path_cubic_to(int id, float c1x, float c1y, float c2x, float c2y, float x, float y);
void  webgfx_path_arc_to(int id, float x, float y, float rx, float ry, float rot, bool large, bool sweep);
void  webgfx_path_close(int id);
void  webgfx_path_bounds(int id, float* out);
bool  webgfx_path_contains(int id, float x, float y, bool even_odd);
void  webgfx_path_transform(int id, float* m);
int   webgfx_canvas_new(int w, int h);
void  webgfx_canvas_free(int id);
void  webgfx_canvas_clear(int id, float* c);
void  webgfx_canvas_save(int id);
void  webgfx_canvas_restore(int id);
void  webgfx_canvas_transform(int id, float* m);
void  webgfx_canvas_clip(int id, int x, int y, int w, int h);
void  webgfx_canvas_clip_rounded_rect(int id, float x, float y, float w, float h, float* r);
void  webgfx_canvas_clip_path(int id, int pid, bool even_odd);
void  webgfx_canvas_clip_out_rect(int id, float x, float y, float w, float h);
void  webgfx_canvas_fill_rect(int id, float x, float y, float w, float h, float* c, float* r);
void  webgfx_canvas_fill_path(int id, int pid, float* c, bool even_odd);
void  webgfx_canvas_stroke_path(int id, int pid, float* c, float width, int cap);
void  webgfx_canvas_glyphs(int id, int font, float px, uint32_t* ids, float* xs, float* ys, int n, float* c);
int   webgfx_image_new(uint8_t* pixels, int w, int h);
void  webgfx_image_update(int id, uint8_t* pixels, int w, int h);
void  webgfx_image_free(int id);
void  webgfx_canvas_draw_image(int id, int img, float* dst, float* uv);
void  webgfx_canvas_draw_canvas(int id, int src, float* dst, float* uv);
void  webgfx_canvas_set_shadow(int id, int i, float* c, float x, float y, float blur, float spread, bool inset);
void  webgfx_canvas_clear_shadows(int id);
void  webgfx_canvas_set_composite(int id, int op);
void  webgfx_canvas_set_filter(int id, float hue, float sat, float bright);
void  webgfx_canvas_compose(int id, int src, int mask, bool luma, float alpha, int x, int y, int w, int h);
void  webgfx_canvas_filter_blur(int id, int src, float x, float y, float sx, float sy, float* tint);
void  webgfx_canvas_read(int id, uint8_t* out);
int   webgfx_share_new(int canvas, int* fd0, int* fd1, int* fmt, int* w, int* h);
int   webgfx_share_publish(int share, int canvas);

// arguments as plain C values
#define ARGS(n) size_t argc = n; napi_value av[n]; \
    napi_get_cb_info(env, info, &argc, av, NULL, NULL)
static int32_t arg_i(napi_env env, napi_value v) { int32_t r = 0; napi_get_value_int32(env, v, &r); return r; }
static uint32_t arg_u(napi_env env, napi_value v) { uint32_t r = 0; napi_get_value_uint32(env, v, &r); return r; }
static double arg_d(napi_env env, napi_value v) { double r = 0; napi_get_value_double(env, v, &r); return r; }
static bool arg_b(napi_env env, napi_value v) { bool r = false; napi_get_value_bool(env, v, &r); return r; }
// a typed array's data, null for null/undefined
static void* arg_p(napi_env env, napi_value v, size_t* len)
{
    napi_valuetype t;
    napi_typeof(env, v, &t);
    if (t == napi_null || t == napi_undefined) return NULL;
    bool is = false;
    napi_is_typedarray(env, v, &is);
    if (is) {
        napi_typedarray_type tt; size_t n; void* data; napi_value ab; size_t off;
        napi_get_typedarray_info(env, v, &tt, &n, &data, &ab, &off);
        if (len) *len = n;
        return data;
    }
    napi_is_buffer(env, v, &is);
    if (is) {
        void* data; size_t n;
        napi_get_buffer_info(env, v, &data, &n);
        if (len) *len = n;
        return data;
    }
    return NULL;
}
static char* arg_s(napi_env env, napi_value v, char* buf, size_t cap)
{
    napi_valuetype t;
    napi_typeof(env, v, &t);
    if (t != napi_string) return NULL;
    size_t n = 0;
    napi_get_value_string_utf8(env, v, buf, cap, &n);
    return buf;
}
static napi_value ret_i(napi_env env, int32_t v) { napi_value r; napi_create_int32(env, v, &r); return r; }
static napi_value ret_d(napi_env env, double v) { napi_value r; napi_create_double(env, v, &r); return r; }
static napi_value ret_b(napi_env env, bool v) { napi_value r; napi_get_boolean(env, v, &r); return r; }
static napi_value ret_none(napi_env env) { napi_value r; napi_get_undefined(env, &r); return r; }
static napi_value ret_f32s(napi_env env, float* v, int n)
{
    napi_value ab, ta; void* data;
    napi_create_arraybuffer(env, (size_t)n * 4, &data, &ab);
    memcpy(data, v, (size_t)n * 4);
    napi_create_typedarray(env, napi_float32_array, (size_t)n, ab, 0, &ta);
    return ta;
}
static void set_num(napi_env env, napi_value o, const char* k, double v)
{
    napi_value n; napi_create_double(env, v, &n); napi_set_named_property(env, o, k, n);
}
static void set_str(napi_env env, napi_value o, const char* k, const char* v)
{
    napi_value s; napi_create_string_utf8(env, v, NAPI_AUTO_LENGTH, &s); napi_set_named_property(env, o, k, s);
}

static napi_value GpuInit(napi_env env, napi_callback_info info) { webgfx_gpu_init(); return ret_none(env); }

// fontMatch(family, weight, width, slope, codepoint, emoji) -> {file, family, index} | null
static napi_value FontMatch(napi_env env, napi_callback_info info)
{
    ARGS(6);
    char fam[256], file[1024], out_fam[256];
    int index = 0;
    char* family = arg_s(env, av[0], fam, sizeof fam);
    bool ok = webgfx_font_match(family, arg_i(env, av[1]), arg_i(env, av[2]), arg_i(env, av[3]),
        arg_u(env, av[4]), arg_b(env, av[5]), (uint8_t*)file, sizeof file, (uint8_t*)out_fam, sizeof out_fam, &index);
    if (!ok) { napi_value r; napi_get_null(env, &r); return r; }
    napi_value o; napi_create_object(env, &o);
    set_str(env, o, "file", file);
    set_str(env, o, "family", out_fam);
    set_num(env, o, "index", index);
    return o;
}
// fontOpen(bytes, index): the bytes are copied; webgfx reads them for life
static napi_value FontOpen(napi_env env, napi_callback_info info)
{
    ARGS(2);
    size_t n = 0;
    void* data = arg_p(env, av[0], &n);
    if (!data || !n) return ret_i(env, 0);
    uint8_t* copy = malloc(n);
    memcpy(copy, data, n);
    return ret_i(env, webgfx_font_open(copy, (int64_t)n, arg_i(env, av[1])));
}
static napi_value FontClose(napi_env env, napi_callback_info info) { ARGS(1); webgfx_font_close(arg_i(env, av[0])); return ret_none(env); }
static napi_value FontGlyphCount(napi_env env, napi_callback_info info) { ARGS(1); return ret_i(env, webgfx_font_glyph_count(arg_i(env, av[0]))); }
static napi_value FontUnitsPerEm(napi_env env, napi_callback_info info) { ARGS(1); return ret_i(env, webgfx_font_units_per_em(arg_i(env, av[0]))); }
static napi_value FontGlyph(napi_env env, napi_callback_info info) { ARGS(2); return ret_d(env, webgfx_font_glyph_index(arg_i(env, av[0]), arg_u(env, av[1]))); }
// fontMetrics(id, px) -> [ascent, descent, xHeight, zeroAdvance]
static napi_value FontMetrics(napi_env env, napi_callback_info info)
{
    ARGS(2);
    float m[4] = { 0, 0, 0, 0 };
    webgfx_font_metrics(arg_i(env, av[0]), (float)arg_d(env, av[1]), m);
    return ret_f32s(env, m, 4);
}
static napi_value FontAdvance(napi_env env, napi_callback_info info)
{
    ARGS(3);
    return ret_d(env, webgfx_font_advance(arg_i(env, av[0]), arg_u(env, av[1]), (float)arg_d(env, av[2])));
}
// fontAdvances(id, px, Uint32Array glyphs, Float32Array out): every advance at once
static napi_value FontAdvances(napi_env env, napi_callback_info info)
{
    ARGS(4);
    size_t n = 0, m = 0;
    uint32_t* g = arg_p(env, av[2], &n);
    float* out = arg_p(env, av[3], &m);
    int id = arg_i(env, av[0]);
    float px = (float)arg_d(env, av[1]);
    if (g && out)
        for (size_t i = 0; i < n && i < m; i++) out[i] = webgfx_font_advance(id, g[i], px);
    return ret_none(env);
}
// fontGlyphs(id, Uint32Array codepoints, Uint32Array out)
static napi_value FontGlyphs(napi_env env, napi_callback_info info)
{
    ARGS(3);
    size_t n = 0, m = 0;
    uint32_t* cps = arg_p(env, av[1], &n);
    uint32_t* out = arg_p(env, av[2], &m);
    int id = arg_i(env, av[0]);
    if (cps && out)
        for (size_t i = 0; i < n && i < m; i++) out[i] = webgfx_font_glyph_index(id, cps[i]);
    return ret_none(env);
}

static napi_value PathNew(napi_env env, napi_callback_info info) { return ret_i(env, webgfx_path_new()); }
static napi_value PathFree(napi_env env, napi_callback_info info) { ARGS(1); webgfx_path_free(arg_i(env, av[0])); return ret_none(env); }
static napi_value PathMoveTo(napi_env env, napi_callback_info info) { ARGS(3); webgfx_path_move_to(arg_i(env, av[0]), (float)arg_d(env, av[1]), (float)arg_d(env, av[2])); return ret_none(env); }
static napi_value PathLineTo(napi_env env, napi_callback_info info) { ARGS(3); webgfx_path_line_to(arg_i(env, av[0]), (float)arg_d(env, av[1]), (float)arg_d(env, av[2])); return ret_none(env); }
static napi_value PathQuadTo(napi_env env, napi_callback_info info)
{
    ARGS(5);
    webgfx_path_quad_to(arg_i(env, av[0]), (float)arg_d(env, av[1]), (float)arg_d(env, av[2]), (float)arg_d(env, av[3]), (float)arg_d(env, av[4]));
    return ret_none(env);
}
static napi_value PathCubicTo(napi_env env, napi_callback_info info)
{
    ARGS(7);
    webgfx_path_cubic_to(arg_i(env, av[0]), (float)arg_d(env, av[1]), (float)arg_d(env, av[2]), (float)arg_d(env, av[3]),
        (float)arg_d(env, av[4]), (float)arg_d(env, av[5]), (float)arg_d(env, av[6]));
    return ret_none(env);
}
// pathArcTo(id, x, y, rx, ry, rotation, large, sweep)
static napi_value PathArcTo(napi_env env, napi_callback_info info)
{
    ARGS(8);
    webgfx_path_arc_to(arg_i(env, av[0]), (float)arg_d(env, av[1]), (float)arg_d(env, av[2]), (float)arg_d(env, av[3]),
        (float)arg_d(env, av[4]), (float)arg_d(env, av[5]), arg_b(env, av[6]), arg_b(env, av[7]));
    return ret_none(env);
}
static napi_value PathClose(napi_env env, napi_callback_info info) { ARGS(1); webgfx_path_close(arg_i(env, av[0])); return ret_none(env); }
static napi_value PathBounds(napi_env env, napi_callback_info info)
{
    ARGS(1);
    float b[4] = { 0, 0, 0, 0 };
    webgfx_path_bounds(arg_i(env, av[0]), b);
    return ret_f32s(env, b, 4);
}
static napi_value PathContains(napi_env env, napi_callback_info info)
{
    ARGS(4);
    return ret_b(env, webgfx_path_contains(arg_i(env, av[0]), (float)arg_d(env, av[1]), (float)arg_d(env, av[2]), arg_b(env, av[3])));
}
static napi_value PathTransform(napi_env env, napi_callback_info info)
{
    ARGS(2);
    float* m = arg_p(env, av[1], NULL);
    if (m) webgfx_path_transform(arg_i(env, av[0]), m);
    return ret_none(env);
}

static napi_value CanvasNew(napi_env env, napi_callback_info info) { ARGS(2); return ret_i(env, webgfx_canvas_new(arg_i(env, av[0]), arg_i(env, av[1]))); }
static napi_value CanvasFree(napi_env env, napi_callback_info info) { ARGS(1); webgfx_canvas_free(arg_i(env, av[0])); return ret_none(env); }
// canvasClear(id, Float32Array rgba)
static napi_value CanvasClear(napi_env env, napi_callback_info info)
{
    ARGS(2);
    float* c = arg_p(env, av[1], NULL);
    float none[4] = { 0, 0, 0, 0 };
    webgfx_canvas_clear(arg_i(env, av[0]), c ? c : none);
    return ret_none(env);
}
static napi_value CanvasSave(napi_env env, napi_callback_info info) { ARGS(1); webgfx_canvas_save(arg_i(env, av[0])); return ret_none(env); }
static napi_value CanvasRestore(napi_env env, napi_callback_info info) { ARGS(1); webgfx_canvas_restore(arg_i(env, av[0])); return ret_none(env); }
// canvasTransform(id, Float32Array [a b c d e f])
static napi_value CanvasTransform(napi_env env, napi_callback_info info)
{
    ARGS(2);
    float* m = arg_p(env, av[1], NULL);
    if (m) webgfx_canvas_transform(arg_i(env, av[0]), m);
    return ret_none(env);
}
static napi_value CanvasClip(napi_env env, napi_callback_info info)
{
    ARGS(5);
    webgfx_canvas_clip(arg_i(env, av[0]), arg_i(env, av[1]), arg_i(env, av[2]), arg_i(env, av[3]), arg_i(env, av[4]));
    return ret_none(env);
}
// canvasClipRounded(id, x, y, w, h, Float32Array radii[4])
static napi_value CanvasClipRounded(napi_env env, napi_callback_info info)
{
    ARGS(6);
    float* r = arg_p(env, av[5], NULL);
    float zero[4] = { 0, 0, 0, 0 };
    webgfx_canvas_clip_rounded_rect(arg_i(env, av[0]), (float)arg_d(env, av[1]), (float)arg_d(env, av[2]),
        (float)arg_d(env, av[3]), (float)arg_d(env, av[4]), r ? r : zero);
    return ret_none(env);
}
static napi_value CanvasClipPath(napi_env env, napi_callback_info info)
{
    ARGS(3);
    webgfx_canvas_clip_path(arg_i(env, av[0]), arg_i(env, av[1]), arg_b(env, av[2]));
    return ret_none(env);
}
static napi_value CanvasClipOutRect(napi_env env, napi_callback_info info)
{
    ARGS(5);
    webgfx_canvas_clip_out_rect(arg_i(env, av[0]), (float)arg_d(env, av[1]), (float)arg_d(env, av[2]), (float)arg_d(env, av[3]), (float)arg_d(env, av[4]));
    return ret_none(env);
}
// canvasFillRect(id, x, y, w, h, Float32Array rgba, Float32Array radii | null)
static napi_value CanvasFillRect(napi_env env, napi_callback_info info)
{
    ARGS(7);
    float* c = arg_p(env, av[5], NULL);
    float* r = arg_p(env, av[6], NULL);
    if (c)
        webgfx_canvas_fill_rect(arg_i(env, av[0]), (float)arg_d(env, av[1]), (float)arg_d(env, av[2]),
            (float)arg_d(env, av[3]), (float)arg_d(env, av[4]), c, r);
    return ret_none(env);
}
static napi_value CanvasFillPath(napi_env env, napi_callback_info info)
{
    ARGS(4);
    float* c = arg_p(env, av[2], NULL);
    if (c) webgfx_canvas_fill_path(arg_i(env, av[0]), arg_i(env, av[1]), c, arg_b(env, av[3]));
    return ret_none(env);
}
// canvasStrokePath(id, pid, rgba, width, cap)
static napi_value CanvasStrokePath(napi_env env, napi_callback_info info)
{
    ARGS(5);
    float* c = arg_p(env, av[2], NULL);
    if (c) webgfx_canvas_stroke_path(arg_i(env, av[0]), arg_i(env, av[1]), c, (float)arg_d(env, av[3]), arg_i(env, av[4]));
    return ret_none(env);
}
// canvasGlyphs(id, font, px, Uint32Array ids, Float32Array xs, Float32Array ys, n, rgba)
static napi_value CanvasGlyphs(napi_env env, napi_callback_info info)
{
    ARGS(8);
    uint32_t* ids = arg_p(env, av[3], NULL);
    float* xs = arg_p(env, av[4], NULL);
    float* ys = arg_p(env, av[5], NULL);
    float* c = arg_p(env, av[7], NULL);
    if (ids && xs && ys && c)
        webgfx_canvas_glyphs(arg_i(env, av[0]), arg_i(env, av[1]), (float)arg_d(env, av[2]), ids, xs, ys, arg_i(env, av[6]), c);
    return ret_none(env);
}
static napi_value ImageNew(napi_env env, napi_callback_info info)
{
    ARGS(3);
    uint8_t* px = arg_p(env, av[0], NULL);
    return ret_i(env, px ? webgfx_image_new(px, arg_i(env, av[1]), arg_i(env, av[2])) : 0);
}
static napi_value ImageUpdate(napi_env env, napi_callback_info info)
{
    ARGS(4);
    uint8_t* px = arg_p(env, av[1], NULL);
    if (px) webgfx_image_update(arg_i(env, av[0]), px, arg_i(env, av[2]), arg_i(env, av[3]));
    return ret_none(env);
}
static napi_value ImageFree(napi_env env, napi_callback_info info) { ARGS(1); webgfx_image_free(arg_i(env, av[0])); return ret_none(env); }
// canvasDrawImage(id, img, Float32Array dst[4], Float32Array uv[4])
static napi_value CanvasDrawImage(napi_env env, napi_callback_info info)
{
    ARGS(4);
    float* dst = arg_p(env, av[2], NULL);
    float* uv = arg_p(env, av[3], NULL);
    float whole[4] = { 0, 0, 1, 1 };
    if (dst) webgfx_canvas_draw_image(arg_i(env, av[0]), arg_i(env, av[1]), dst, uv ? uv : whole);
    return ret_none(env);
}
static napi_value CanvasDrawCanvas(napi_env env, napi_callback_info info)
{
    ARGS(4);
    float* dst = arg_p(env, av[2], NULL);
    float* uv = arg_p(env, av[3], NULL);
    float whole[4] = { 0, 0, 1, 1 };
    if (dst) webgfx_canvas_draw_canvas(arg_i(env, av[0]), arg_i(env, av[1]), dst, uv ? uv : whole);
    return ret_none(env);
}
// canvasSetShadow(id, slot, rgba, x, y, blur, spread, inset)
static napi_value CanvasSetShadow(napi_env env, napi_callback_info info)
{
    ARGS(8);
    float* c = arg_p(env, av[2], NULL);
    if (c)
        webgfx_canvas_set_shadow(arg_i(env, av[0]), arg_i(env, av[1]), c, (float)arg_d(env, av[3]), (float)arg_d(env, av[4]),
            (float)arg_d(env, av[5]), (float)arg_d(env, av[6]), arg_b(env, av[7]));
    return ret_none(env);
}
static napi_value CanvasClearShadows(napi_env env, napi_callback_info info) { ARGS(1); webgfx_canvas_clear_shadows(arg_i(env, av[0])); return ret_none(env); }
static napi_value CanvasSetComposite(napi_env env, napi_callback_info info) { ARGS(2); webgfx_canvas_set_composite(arg_i(env, av[0]), arg_i(env, av[1])); return ret_none(env); }
static napi_value CanvasSetFilter(napi_env env, napi_callback_info info)
{
    ARGS(4);
    webgfx_canvas_set_filter(arg_i(env, av[0]), (float)arg_d(env, av[1]), (float)arg_d(env, av[2]), (float)arg_d(env, av[3]));
    return ret_none(env);
}
// canvasCompose(id, src, mask, luma, alpha, x, y, w, h)
static napi_value CanvasCompose(napi_env env, napi_callback_info info)
{
    ARGS(9);
    webgfx_canvas_compose(arg_i(env, av[0]), arg_i(env, av[1]), arg_i(env, av[2]), arg_b(env, av[3]), (float)arg_d(env, av[4]),
        arg_i(env, av[5]), arg_i(env, av[6]), arg_i(env, av[7]), arg_i(env, av[8]));
    return ret_none(env);
}
// canvasFilterBlur(id, src, x, y, sx, sy, tint | null)
static napi_value CanvasFilterBlur(napi_env env, napi_callback_info info)
{
    ARGS(7);
    float* tint = arg_p(env, av[6], NULL);
    webgfx_canvas_filter_blur(arg_i(env, av[0]), arg_i(env, av[1]), (float)arg_d(env, av[2]), (float)arg_d(env, av[3]),
        (float)arg_d(env, av[4]), (float)arg_d(env, av[5]), tint);
    return ret_none(env);
}
// canvasRead(id, Buffer out)
static napi_value CanvasRead(napi_env env, napi_callback_info info)
{
    ARGS(2);
    uint8_t* out = arg_p(env, av[1], NULL);
    if (out) webgfx_canvas_read(arg_i(env, av[0]), out);
    return ret_none(env);
}

// shareNew(canvas) -> {id, fd0, fd1, fmt, w, h} | null
static napi_value ShareNew(napi_env env, napi_callback_info info)
{
    ARGS(1);
    int fd0 = -1, fd1 = -1, fmt = 0, w = 0, h = 0;
    int id = webgfx_share_new(arg_i(env, av[0]), &fd0, &fd1, &fmt, &w, &h);
    if (!id) { napi_value r; napi_get_null(env, &r); return r; }
    napi_value o; napi_create_object(env, &o);
    set_num(env, o, "id", id);
    set_num(env, o, "fd0", fd0);
    set_num(env, o, "fd1", fd1);
    set_num(env, o, "fmt", fmt);
    set_num(env, o, "w", w);
    set_num(env, o, "h", h);
    return o;
}
static napi_value SharePublish(napi_env env, napi_callback_info info) { ARGS(2); return ret_i(env, webgfx_share_publish(arg_i(env, av[0]), arg_i(env, av[1]))); }

// sendFds(sock, text, [fds]): the text and the fds in one message
static napi_value SendFds(napi_env env, napi_callback_info info)
{
    ARGS(3);
    int sock = arg_i(env, av[0]);
    char text[4096];
    arg_s(env, av[1], text, sizeof text);
    uint32_t n = 0;
    napi_get_array_length(env, av[2], &n);
    if (n > 8) n = 8;
    int fds[8];
    for (uint32_t i = 0; i < n; i++) {
        napi_value e; napi_get_element(env, av[2], i, &e);
        fds[i] = arg_i(env, e);
    }
    struct iovec iov = { text, strlen(text) };
    union { struct cmsghdr h; char b[CMSG_SPACE(sizeof(int) * 8)]; } ctl;
    memset(&ctl, 0, sizeof ctl);
    struct msghdr msg = { 0 };
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = &ctl;
    msg.msg_controllen = CMSG_SPACE(sizeof(int) * n);
    struct cmsghdr* c = CMSG_FIRSTHDR(&msg);
    c->cmsg_level = SOL_SOCKET;
    c->cmsg_type = SCM_RIGHTS;
    c->cmsg_len = CMSG_LEN(sizeof(int) * n);
    memcpy(CMSG_DATA(c), fds, sizeof(int) * n);
    return ret_b(env, sendmsg(sock, &msg, MSG_NOSIGNAL) == (ssize_t)iov.iov_len);
}

#define FN(name, f) { name, NULL, f, NULL, NULL, NULL, napi_default, NULL }
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor d[] = {
        FN("gpuInit", GpuInit),
        FN("fontMatch", FontMatch), FN("fontOpen", FontOpen), FN("fontClose", FontClose),
        FN("fontGlyphCount", FontGlyphCount), FN("fontUnitsPerEm", FontUnitsPerEm),
        FN("fontGlyph", FontGlyph), FN("fontGlyphs", FontGlyphs), FN("fontMetrics", FontMetrics),
        FN("fontAdvance", FontAdvance), FN("fontAdvances", FontAdvances),
        FN("pathNew", PathNew), FN("pathFree", PathFree), FN("pathMoveTo", PathMoveTo),
        FN("pathLineTo", PathLineTo), FN("pathQuadTo", PathQuadTo), FN("pathCubicTo", PathCubicTo),
        FN("pathArcTo", PathArcTo), FN("pathClose", PathClose), FN("pathBounds", PathBounds),
        FN("pathContains", PathContains), FN("pathTransform", PathTransform),
        FN("canvasNew", CanvasNew), FN("canvasFree", CanvasFree), FN("canvasClear", CanvasClear),
        FN("canvasSave", CanvasSave), FN("canvasRestore", CanvasRestore), FN("canvasTransform", CanvasTransform),
        FN("canvasClip", CanvasClip), FN("canvasClipRounded", CanvasClipRounded), FN("canvasClipPath", CanvasClipPath),
        FN("canvasClipOutRect", CanvasClipOutRect), FN("canvasFillRect", CanvasFillRect),
        FN("canvasFillPath", CanvasFillPath), FN("canvasStrokePath", CanvasStrokePath), FN("canvasGlyphs", CanvasGlyphs),
        FN("imageNew", ImageNew), FN("imageUpdate", ImageUpdate), FN("imageFree", ImageFree),
        FN("canvasDrawImage", CanvasDrawImage), FN("canvasDrawCanvas", CanvasDrawCanvas),
        FN("canvasSetShadow", CanvasSetShadow), FN("canvasClearShadows", CanvasClearShadows),
        FN("canvasSetComposite", CanvasSetComposite), FN("canvasSetFilter", CanvasSetFilter),
        FN("canvasCompose", CanvasCompose), FN("canvasFilterBlur", CanvasFilterBlur), FN("canvasRead", CanvasRead),
        FN("shareNew", ShareNew), FN("sharePublish", SharePublish), FN("sendFds", SendFds),
    };
    napi_define_properties(env, exports, sizeof d / sizeof d[0], d);
    return exports;
}
NAPI_MODULE_INIT() { return Init(env, exports); }
