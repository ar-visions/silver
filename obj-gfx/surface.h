#ifndef _GFX_SURFACE_
#define _GFX_SURFACE_

#define _Surface(D,T,C) _Base(spr,T,C)  \
    override(D,T,C,String,to_string,(C)) \
    method(D,T,C,int,divisible,(int n, int d)) \
    method(D,T,C,uint,framebuffer,(C)) \
    method(D,T,C,C,cache_fetch,(Gfx gfx, int w, int h, enum SurfaceType type)) \
    method(D,T,C,void,cache_update,(C)) \
    method(D,T,C,C,alloc,(Gfx gfx, int w, int h, int stride, enum SurfaceType type)) \
    method(D,T,C,void,texture_clamp,(C, bool)) \
    method(D,T,C,C,resample,(C, int w, int h, bool flip)) \
    method(D,T,C,uchar *,image_from_file,(char *filename, int *w, int *h, int *c, int *stride)) \
    method(D,T,C,C,image_with_bytes,(Gfx gfx, uchar *bytes, int length, bool store)) \
    method(D,T,C,bool,write,(C, char *filename)) \
    method(D,T,C,bool,write_callback,(C, void(*callback_func)(void *, void *, int), void *context)) \
    method(D,T,C,int,channels,(C)) \
    method(D,T,C,int,stride,(C)) \
    method(D,T,C,void,update_rgba,(C, int w, int h, RGBA *rgba, int stride, bool store)) \
    method(D,T,C,void,yuvp,(Gfx gfx, int w, int h, uchar *y_plane, int y_stride, \
        uchar *u_plane, int u_stride, uchar *v_plane, int v_stride)) \
    method(D,T,C,void,update_yuvp,(C, int w, int h, \
    uchar *y_plane, int y_stride, uchar *u_plane, int u_stride, uchar *v_plane, int v_stride)) \
    method(D,T,C,C,create_yuvp,(Gfx *gfx, int w, int h, \
    uchar *y_plane, int y_stride, uchar *u_plane, int u_stride, uchar *v_plane, int v_stride)) \
    method(D,T,C,GLenum,gl_type,(C)) \
    method(D,T,C,void,readable,(C, bool readable)) \
    method(D,T,C,int,read,(C)) \
    method(D,T,C,bool,resize,(C, int w, int h)) \
    method(D,T,C,void,linear,(C, bool linear)) \
    method(D,T,C,C,new_gray,(Gfx gfx, int w, int h, uchar *bytes, int stride, bool store)) \
    method(D,T,C,C,new_rgba,(Gfx gfx, int w, int h, RGBA *bytes, int stride, bool store)) \
    method(D,T,C,C,new_rgba_empty,(Gfx gfx, int w, int h)) \
    override(D,T,C,void,free,(C)) \
    method(D,T,C,void,dest,(C)) \
    method(D,T,C,void,src,(C, float x, float y)) \
    private_var(D,T,C,Gfx,gfx)          \
    private_var(D,T,C,uint,tx)          \
    private_var(D,T,C,int,w)            \
    private_var(D,T,C,int,h)            \
    private_var(D,T,C,bool,image)       \
    private_var(D,T,C,uint,framebuffer) \
    private_var(D,T,C,uint,ppo)         \
    private_var(D,T,C,int,ppo_len)      \
    private_var(D,T,C,uchar *,bytes)    \
    private_var(D,T,C,int,stride)       \
    private_var(D,T,C,enum SurfaceType,type) \
    private_var(D,T,C,long,cached_at)
declare(Surface,Base)

#endif