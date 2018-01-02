#ifndef _GFX_FONT_

typedef struct _GfxTextExtents {
    double x, y;
    double ascent, decent;
} GfxTextExtents;

typedef struct _GfxMeasureTextArgs {
	double x, y;
} GfxMeasureTextArgs;

#define _Font(D,T,C) _Base(spr,T,C)                 \
    override(D,T,C,void,init,(C))                   \
    override(D,T,C,void,free,(C))                   \
    override(D,T,C,void,class_init,(Class))         \
    method(D,T,C,C,with_ttf,(Gfx, const char *, ushort))  \
    method(D,T,C,bool,save_db,(Gfx, char *))        \
    method(D,T,C,bool,load_db,(Gfx, char *))        \
    method(D,T,C,void,transfer_surfaces,(C, Gfx))   \
    var(D,T,C,String,family_name)                   \
    var(D,T,C,String,file_name)                     \
    var(D,T,C,ushort,point_size)                    \
    var(D,T,C,List,glyph_sets)                      \
    var(D,T,C,List,surface_data)                    \
    var(D,T,C,List,surfaces)                        \
    var(D,T,C,int,glyph_total)                      \
    var(D,T,C,int,ascent)                           \
    var(D,T,C,int,descent)                          \
    var(D,T,C,int,height)                           \
    var(D,T,C,int,max_surfaces)                     \
    var(D,T,C,bool,from_database)                   \
    var(D,T,C,uint8 *,pixels)                       \
    var(D,T,C,struct _object_Font *,scaled)
declare(Font, Base)

#define _Fonts(D,T,C) _Base(spr,T,C)                \
    override(D,T,C,void,init,(C))                   \
    method(D,T,C,bool,save,(C, const char *))       \
    method(D,T,C,C,load,(const char *))             \
    method(D,T,C,Font,find,(C, const char *))       \
    var(D,T,C,List,fonts)
declare(Fonts, Base)

#define _Glyph(D,T,C) _Base(spr,T,C)                \
    override(D,T,C,void,init,(C))                   \
    var(D,T,C,int,w)                                \
    var(D,T,C,int,h)                                \
    var(D,T,C,int,x_offset)                         \
    var(D,T,C,int,y_offset)                         \
    var(D,T,C,int,advance)                          \
    var(D,T,C,int,surface_index)                    \
	var(D,T,C,Vec,uv)
declare(Glyph,Base)

#define _CharRange(D,T,C) _Base(spr,T,C)            \
    override(D,T,C,uint,hash,(C))                   \
    method(D,T,C,C,new_range,(int, int, const char *)) \
    method(D,T,C,C,with_string,(String))            \
    method(D,T,C,C,find,(uint))                     \
    private_var(D,T,C,String,name)                  \
	private_var(D,T,C,uint,from)                    \
	private_var(D,T,C,uint,to)
declare(CharRange,Base)

#define _GlyphSet(D,T,C) _Base(spr,T,C)             \
	var(D,T,C,CharRange,range)                      \
    var(D,T,C,List,glyphs)
declare(GlyphSet,Base)

#endif