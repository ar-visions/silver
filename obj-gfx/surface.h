#ifndef _GFX_SURFACE_
#define _GFX_SURFACE_

#define _Surface(D,T,C) _Base(spr,T,C)  \
    override(D,T,C,String,to_string,(C)) \
    override(D,T,C,void,free,(C)) \
    method(D,T,C,int,divisible,(int n, int d)) \
    method(D,T,C,uint,framebuffer,(C)) \
    method(D,T,C,C,cache_fetch,(C, int w, int h, enum SurfaceType type)) \
    method(D,T,C,void,cache_update,(C)) \
    method(D,T,C,C,alloc,(C, int w, int h, int stride, enum SurfaceType type)) \
    method(D,T,C,void,texture_clamp,(C, bool)) \
    method(D,T,C,C,resample,(C, int w, int h, bool flip)) \
    method(D,T,C,uint8 *,image_from_file,(char *filename, int *w, int *h, int *c, int *stride)) \
    method(D,T,C,C,image_with_bytes,(C, uint8 *bytes, int length, bool store)) \
    method(D,T,C,bool,write,(C, char *filename)) \
    method(D,T,C,bool,write_callback,(C, void(*callback_func)(void *, void *, int), void *context)) \
    method(D,T,C,int,channels,(C)) \
    method(D,T,C,int,stride,(C)) \
    method(D,T,C,void,update_rgba,(C, int w, int h, RGBA *rgba, int stride, bool store)) \
    method(D,T,C,void,yuvp,(C, int w, int h, uint8 *y_plane, int y_stride, \
        uint8 *u_plane, int u_stride, uint8 *v_plane, int v_stride)) \
    method(D,T,C,void,update_yuvp,(C, int w, int h, \
        uint8 *y_plane, int y_stride, uint8 *u_plane, int u_stride, uint8 *v_plane, int v_stride)) \
    method(D,T,C,C,create_yuvp,(Gfx *gfx, int w, int h, \
        uint8 *y_plane, int y_stride, uint8 *u_plane, int u_stride, uint8 *v_plane, int v_stride)) \
    method(D,T,C,GLenum,gl_type,(C)) \
    method(D,T,C,void,readable,(C, bool readable)) \
    method(D,T,C,int,read,(C)) \
    method(D,T,C,bool,resize,(C, int w, int h)) \
    method(D,T,C,void,linear,(C, bool linear)) \
    method(D,T,C,C,new_gray,(C, int w, int h, uint8 *bytes, int stride, bool store)) \
    method(D,T,C,C,new_rgba,(C, int w, int h, RGBA *bytes, int stride, bool store)) \
    method(D,T,C,C,new_rgba_empty,(C, int w, int h)) \
    method(D,T,C,void,dest,(C)) \
    method(D,T,C,void,src,(C, float x, float y)) \
    private_var(D,T,C,Gfx,gfx)          \
    private_var(D,T,C,uint,tx)          \
    private_var(D,T,C,int,w)            \
    private_var(D,T,C,int,h)            \
    private_var(D,T,C,bool,image)       \
    private_var(D,T,C,uint,fb)          \
    private_var(D,T,C,uint,ppo)         \
    private_var(D,T,C,int,ppo_len)      \
    private_var(D,T,C,uint8 *,bytes)    \
    private_var(D,T,C,int,stride)       \
    private_var(D,T,C,enum SurfaceType,type) \
    private_var(D,T,C,long,cached_at)
declare(Surface,Base)

#define _Clip(D,T,C) _Base(spr,T,C)                 \
	private_var(D,T,C,Surface,surface)              \
	private_var(D,T,C,Surface,last_surface)         \
	private_var(D,T,C,GfxRect,rect)                 \
	private_var(D,T,C,float2,scale)                 \
	private_var(D,T,C,float2,offset)                \
	private_var(D,T,C,float,u[4])
declare(Clip,Base)

#endif