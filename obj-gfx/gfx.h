#ifndef _GFX_
#define _GFX_

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

typedef struct _RGBA {
	u_char r, g, b, a;
} RGBA;

typedef struct _RGB {
	u_char r, g, b;
} RGB;

#include "shape.h"

typedef struct _GfxSync {
#ifndef EMSCRIPTEN
	pthread_t owner;
	pthread_mutex_t mutex;
	int lock_count;
#endif
    bool placeholder;
} GfxSync;

typedef struct _ResampleArgs {
	float amount;
} ResampleArgs;

typedef struct _GaussianArgs {
	float reduction_sx, reduction_sy;
	float weights[9];
	float offsets[8];
} GaussianArgs;

typedef struct _LinearArgs {
	Surface *src;
	float src_blend;
	GfxColor color_from;
	GfxColor color_to;
	V2 from;
	V2 to;
} LinearArgs;

typedef struct _TextExtents {
	float w;
	float h;
	float ascent;
	float descent;
} TextExtents;

#define _GfxState(D,T,C) _Base(spr,T,C)             \
    private_var(D,T,C,int w)                        \
    private_var(D,T,C,int,h)                        \
    private_var(D,T,C,float2,cursor)                \
    private_var(D,T,C,bool,moved)                   \
    private_var(D,T,C,bool,new_path)                \
    private_var(D,T,C,Color,color)                  \
    private_var(D,T,C,Color,text_color)             \
    private_var(D,T,C,Mat44,mat)                    \
    private_var(D,T,C,Mat44,proj)                   \
    private_var(D,T,C,float2,surface_v00)           \
    private_var(D,T,C,float2,surface_v10)           \
    private_var(D,T,C,float2,surface_v11)           \
    private_var(D,T,C,float2,surface_v01)           \
    private_var(D,T,C,float,feather)                \
    private_var(D,T,C,float,feather_stroke_factor)  \
    private_var(D,T,C,float,stroke_width)           \
    private_var(D,T,C,float,stroke_scale)           \
    private_var(D,T,C,float,miter_limit)            \
    private_var(D,T,C,Surface,surface_src)       \
    private_var(D,T,C,Surface,surface_dst)       \
    private_var(D,T,C,enum StrokeJoin,stroke_join)  \
    private_var(D,T,C,enum StrokeCap,stroke_cap)    \
    private_var(D,T,C,Font,font)                    \
    private_var(D,T,C,float,letter_spacing)         \
    private_var(D,T,C,float,line_scale)             \
    private_var(D,T,C,Color,linear_color_from)      \
    private_var(D,T,C,Color,linear_color_to)        \
    private_var(D,T,C,float2,linear_from)           \
    private_var(D,T,C,float2,linear_to)             \
    private_var(D,T,C,float,linear_blend)           \
    private_var(D,T,C,float,linear_multiply)        \
    private_var(D,T,C,Surface,clip)              \
    private_var(D,T,C,float,opacity)                \
    private_var(D,T,C,float,prev_opacity)           \
    private_var(D,T,C,bool,arb_rotation)            \
    private_var(D,T,C,GfxState,state)               \
    private_var(D,T,C,List,stack)                   \
    private_var(D,T,C,float2,path)                  \
    private_var(D,T,C,float2,clips)                 \
    private_var(D,T,C,float2,size)                  \
    private_var(D,T,C,Pairs,shaders)                \
    private_var(D,T,C,uint,vbo)                     \
    private_var(D,T,C,VertexNew *,vbuffer)          \
    private_var(D,T,C,List,fonts)                   \
    private_var(D,T,C,List,surface_cache)           \
    private_var(D,T,C,int,surface_cache_size)       \
    private_var(D,T,C,bool,debug)                   \
    private_var(D,T,C,GfxSync,sync)
declare(GfxState, Base)

#define _Gfx(D,T,C) _Base(spr,T,C)                  \
    override(D,T,C,void,init,(C))                   \
    override(D,T,C,void,free,(C))                   \
    method(D,T,C,u_char *,image_from_file,(char *, int *, int *, int *, int *)) \
    method(D,T,C,C,new_with_size,(int, int))        \
    method(D,T,C,int,defaults,(C))                  \
    method(D,T,C,void,divisible,(int, int))         \
    method(D,T,C,void,clear,(C, Color))             \
    method(D,T,C,void,fill,(C,bool))                \
    method(D,T,C,void,rounded_rect,(C,float,float,float,float,float)) \
    method(D,T,C,float,scaled_feather,(C))          \
    method(D,T,C,void,push,(C))                     \
    method(D,T,C,void,pop,(C))                      \
    method(D,T,C,void,ident,(C))                    \
    method(D,T,C,void,rotate,(C,float))             \
    method(D,T,C,void,scale,(C,float,float))        \
    method(D,T,C,void,translate,(C,float,float))    \
    method(D,T,C,void,feather,(C,float)) \
    method(D,T,C,void,debug,(C,bool)) \
    method(D,T,C,void,realloc_buffer,(C,int)) \
    method(D,T,C,void,stroke_cap,(C, enum StrokeCap)) \
    method(D,T,C,void,stroke_join,(C, enum StrokeJoin)) \
    method(D,T,C,void,stroke_miter,(C, float)) \
    method(D,T,C,void,surface_source,(C, struct _object_Surface *)) \
    method(D,T,C,void,surface_dest,(C, struct _object_Surface *)) \
    method(D,T,C,int,fill_verts,(C, int *, enum ShaderType *, bool *)) \
    method(D,T,C,void,font_select,(Gfx, char *, u_short)) \
    method(D,T,C,void,get_matrix,(C, Mat44)) \
    method(D,T,C,void,set_matrix,(C, Mat44)) \
    method(D,T,C,void,draw_text,(C, char *, int, Color, u_char *)) \
    method(D,T,C,void,text_color,(C, Color)) \
    method(D,T,C,void,text_extents,(C, char *, int, TextExtents *)) \
    method(D,T,C,bool,to_screen,(C, float, float, float *, float *)) \
    method(D,T,C,bool,from_screen,(C, float, float, float *, float *)) \
    method(D,T,C,void,color,(C, Color)) \
    method(D,T,C,void,linear_color_from,(C, Color)) \
    method(D,T,C,void,linear_color_to,(C, Color)) \
    method(D,T,C,void,linear_from,(C, float, float)) \
    method(D,T,C,void,linear_to,(C, float, float)) \
    method(D,T,C,void,linear_blend,(C, float)) \
    method(D,T,C,void,linear_multiply,(C, float)) \
    method(D,T,C,void,stroke_width,(C, float)) \
    method(D,T,C,void,stroke_scale,(C, float)) \
    method(D,T,C,void,new_path,(C)) \
    method(D,T,C,void,new_sub_path,(C)) \
    method(D,T,C,float2,arc_seg,(C, Segment *, float2 *, float, float, float, float, float)) \
    method(D,T,C,void,arc_to,(C, float, float, float, float, float)) \
    method(D,T,C,void,rect_to,(C, float, float, float, float, float)) \
    method(D,T,C,void,bezier_to,(C, float, float, float, float, float, float)) \
    method(D,T,C,void,rect,(C, float, float, float, float)) \
    method(D,T,C,void,line_to,(C, float, float)) \
    method(D,T,C,void,move_to,(C, float, float)) \
    method(D,T,C,void,close_path,(C)) \
    method(D,T,C,void,clip,(C, bool)) \
    method(D,T,C,void,clip_stroke,(C, bool)) \
    method(D,T,C,bool,unclip,(C)) \
    method(D,T,C,int,stroke_shape,(C)) \
    method(D,T,C,void,stroke,(C, bool)) \
    method(D,T,C,void,render_pass,(C, int, enum ShaderType, int, bool)) \
    method(D,T,C,void,letter_spacing,(C, float)) \
    method(D,T,C,void,line_scale,(C, float)) \
    method(D,T,C,void,gaussian,(C, float)) \
    method(D,T,C,long,millis,()) \
    method(D,T,C,void,resize,(C, int, int)) \
    method(D,T,C,void,projection,(C, float, float, float, float)) \
    method(D,T,C,void,yuvp,(C, int, int, \
		u_char *, int , u_char *, int, u_char *, int)) \
    method(D,T,C,void,get_scales,(C, float *, float *)) \
    method(D,T,C,void,opacity,(C, float)) \
    method(D,T,C,void,text_ellipsis,(C, char *, int, char *, int, TextExtents *)) \
    method(D,T,C,void,draw_text_ellipsis,(C, char *, int, int)) \
    method(D,T,C,List,lines_from_path,(Gfx, List, bool)) \
    method(D,T,C,bool,is_rect_path,(Gfx, List, float *, float *, GfxRect *)) \
    method(D,T,C,void,path_bbox,(Gfx, List, GfxRect *)) \
    var(D,T,C,GfxState,state)               \
    var(D,T,C,List,stack)                   \
    var(D,T,C,float2,path)                  \
    var(D,T,C,float2,clips)                 \
    var(D,T,C,float2,size)                  \
    var(D,T,C,Pairs,shaders)                \
    var(D,T,C,uint,vbo)                     \
    var(D,T,C,VertexNew *,vbuffer)          \
    var(D,T,C,List,fonts)                   \
    var(D,T,C,List,surface_cache)           \
    var(D,T,C,int,surface_cache_size)       \
    var(D,T,C,bool,debug)                   \
    var(D,T,C,GfxSync,sync)
declare(Gfx, Base)

#define _Surface(D,T,C) _Base(spr,T,C)           \
    override(D,T,C,void,free,(C))                   \
    method(D,T,C,C,new_with_size,(Gfx, int, int))   \
    method(D,T,C,C,new_yuvp,(Gfx, int, int, u_char *, int, u_char *, int, u_char *, int)) \
    method(D,T,C,C,new_rgba,(Gfx, int, int, RGBA *, int, bool)) \
    method(D,T,C,C,new_gray,(Gfx,int,int,u_char *, int, bool)) \
    method(D,T,C,void,update_yuvp,(C, int, int, u_char *, int, u_char *, int, u_char *, int)) \
    method(D,T,C,void,update_rgba,(C,int,int,RGBA *,int,bool)) \
    method(D,T,C,C,new_with_image,(Gfx, char *, bool)) \
    method(D,T,C,C,new_with_image_bytes,(Gfx, u_char *, int, bool)) \
    method(D,T,C,C,resample,(C, int, int, bool)) \
    method(D,T,C,C,cache_fetch,(Gfx, int, int, enum SurfaceType)) \
    method(D,T,C,int,get_channels,(C))              \
    method(D,T,C,int,get_stride,(C))                \
    method(D,T,C,bool,resize,(C,int,int))           \
    method(D,T,C,void,cache_update,(C))             \
    method(D,T,C,void,texture_clamp,(C, bool))      \
    method(D,T,C,void,destroy,(C))                  \
    method(D,T,C,uint,get_framebuffer,(C))          \
    method(D,T,C,void,readable,(C,bool))            \
    method(D,T,C,void,linear,(C,bool))              \
    method(D,T,C,int,read,(C))                      \
    method(D,T,C,bool,write,(C,char *))             \
    method(D,T,C,bool,write_callback,(C,void(*)(void *, void *, int), void *)) \
    var(D,T,C,uint,tx)                              \
    var(D,T,C,int,w)                                \
    var(D,T,C,int,h)                                \
    var(D,T,C,bool,image)                           \
    var(D,T,C,uint,framebuffer)                     \
    var(D,T,C,uint,ppo)                             \
    var(D,T,C,int,ppo_len)                          \
    var(D,T,C,u_char *,bytes)                       \
    var(D,T,C,int,stride)                           \
    var(D,T,C,enum SurfaceType,type)                \
    var(D,T,C,long,cached_at)
declare(Surface, Base)

#define _Clip(D,T,C) _Base(spr,T,C)                 \
	private_var(D,T,C,Surface,surface)              \
	private_var(D,T,C,Surface,last_surface)         \
	private_var(D,T,C,GfxRect rect)                 \
	private_var(D,T,C,float2,scale)                 \
	private_var(D,T,C,float2,offset)                \
	private_var(D,T,C,float,u[4])
declare(Clip,Base)

#include <obj-gfx/font.h>

#endif