#define GLFW_INCLUDE_ES3
#include <GLFW/glfw3.h>
#include <obj-poly2tri/poly2tri.h>
#include <obj-math/math.h>

#if !defined(_XOPEN_SOURCE) || (_XOPEN_SOURCE < 600)
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif
#endif

#include <GLES3/gl3.h>
#ifdef EMSCRIPTEN
#	include <emscripten/emscripten.h>
#endif
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <memory.h>
#include <obj-gfx/shaders.h>
#include <obj-gfx/color.h>
#include <obj-gfx/drawing.h>
#include <obj-gfx/stroke.h>
#ifndef EMSCRIPTEN
#include <pthread.h>
#endif

enum SurfaceType {
	SURFACE_RGBA,
	SURFACE_GRAY,
	SURFACE_YUVP
};

enum SurfaceFilter {
	GFX_FILTER_NEAREST,
	GFX_FILTER_LINEAR
};

struct RGBA {
	u_char r, g, b, a;
};

struct RGB {
	u_char r, g, b;
};

#include "shape.h"

struct GfxSync {
#ifndef EMSCRIPTEN
	pthread_t owner;
	pthread_mutex_t mutex;
	int lock_count;
#endif
    bool placeholder;
};

struct ResampleArgs {
	float amount;
};

struct GaussianArgs {
	float reduction_sx, reduction_sy;
	float weights[9];
	float offsets[8];
};

struct LinearArgs {
	struct _object_Surface *src;
	float src_blend;
	struct _object_Color *color_from;
	struct _object_Color *color_to;
	float2 from;
	float2 to;
};

struct TextExtents {
	float w;
	float h;
	float ascent;
	float descent;
};

forward Mat44, Surface, Fonts;

class GfxState {
    private int w;
    private int h;
    private float2 cursor;
    private bool moved;
    private bool new_path;
    private Color color;
    private Color text_color;
    private Mat44,mat;
    private Mat44,proj;
    private float2 surface_v00;
    private float2 surface_v10;
    private float2 surface_v11;
    private float2 surface_v01;
    private float feather;
    private float feather_stroke_factor;
    private float stroke_width;
    private float stroke_scale;
    private float miter_limit;
    private Surface surface_src;
    private Surface surface_dst;
    private enum StrokeJoin stroke_join;
    private enum StrokeCap stroke_cap;
    private Font font;
    private float letter_spacing;
    private float line_scale;
    private Color linear_color_from;
    private Color linear_color_to;
    private float2 linear_from;
    private float2 linear_to;
    private float linear_blend;
    private float linear_multiply;
    private Surface clip;
    private float opacity;
    private float prev_opacity;
    private bool arb_rotation;
    private GfxState state;
    private List stack;
    private float2 path;
    private float2 clips;
    private float2 size;
    private Pairs shaders;
    private uint vbo;
    private VertexNew *vbuffer;
    private List surface_cache;
    private int surface_cache_size;
    private bool debug;
    private GfxSync sync;
};

class Gfx {
    override void init(C);
    override void free(C);
    u_char * image_from_file(char *, int *, int *, int *, int *);
    C new_with_size(int, int);
    int defaults(C);
    void divisible(int, int);
    void clear(C, Color);
    void fill(C,bool);
    void rounded_rect(C,float,float,float,float,float);
    float scaled_feather(C);
    void push(C);
    void pop(C);
    void ident(C);
    void rotate(C,float);
    void scale(C,float,float);
    void translate(C,float,float);
    void feather(C,float);
    void debug(C,bool);
    void realloc_buffer(C,int);
    void stroke_cap(C, enum StrokeCap);
    void stroke_join(C, enum StrokeJoin);
    void stroke_miter(C, float);
    void surface_source(C, Surface);
    void surface_dest(C, Surface);
    int fill_verts(C, int *, enum ShaderType *, bool *);
    void font_select(Gfx, char *, u_short);
    void get_matrix(C, Mat44);
    void set_matrix(C, Mat44);
    void draw_text(C, char *, int, Color, u_char *);
    void text_color(C, Color);
    void text_extents(C, char *, int, TextExtents *);
    bool to_screen(C, float, float, float *, float *);
    bool from_screen(C, float, float, float *, float *);
    void color(C, Color);
    void linear_color_from(C, Color);
    void linear_color_to(C, Color);
    void linear_from(C, float, float);
    void linear_to(C, float, float);
    void linear_blend(C, float);
    void linear_multiply(C, float);
    void stroke_width(C, float);
    void stroke_scale(C, float);
    void new_path(C);
    void new_sub_path(C);
    float2 arc_seg(C, Segment *, float2 *, float, float, float, float, float);
    void arc_to(C, float, float, float, float, float);
    void rect_to(C, float, float, float, float, float);
    void bezier_to(C, float, float, float, float, float, float);
    void rect(C, float, float, float, float);
    void line_to(C, float, float);
    void move_to(C, float, float);
    void close_path(C);
    void clip(C, bool);
    void clip_stroke(C, bool);
    bool unclip(C);
    int stroke_shape(C);
    void stroke(C, bool);
    void render_pass(C, int, enum ShaderType, int, bool);
    void letter_spacing(C, float);
    void line_scale(C, float);
    void gaussian(C, float);
    long millis();
    void resize(C, int, int);
    void projection(C, float, float, float, float);
    void yuvp(C, int, int,
		u_char *, int , u_char *, int, u_char *, int);
    void get_scales(C, float *, float *);
    void opacity(C, float);
    void text_ellipsis(C, char *, int, char *, int, TextExtents *);
    void draw_text_ellipsis(C, char *, int, int);
    List lines_from_path(Gfx, List, bool);
    bool is_rect_path(Gfx, List, float *, float *, GfxRect *);
    void path_bbox(Gfx, List, GfxRect *);
    int shaders_init(C);
    void shaders_use(C, enum ShaderType type, void *shader_args, bool clipping_op)) \
    void feather_ab(C, enum ShaderType type, float a, float b);
    void clip_surface(C, enum ShaderType type, struct _GfxClip *clip);
    Shader * shaders_lookup(C, enum ShaderType type);
    private GfxState state;
    private List stack;
    private float2 path;
    private float2 clips;
    private float2 size;
    private Pairs shaders;
    private uint vbo;
    private VertexNew * vbuffer;
    private Fonts fonts;
    private List surface_cache;
    private int surface_cache_size;
    private bool debug;
    private GfxSync sync;
}

#include <obj-gfx/surface.h>
#include <obj-gfx/font.h>
