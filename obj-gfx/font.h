#ifndef _GFX_FONT_

#define _Fonts(D,T,C) _Base(spr,T,C)                \
    override(D,T,C,void,init,(C))                   \
    method(D,T,C,bool,save,(C, const char *))       \
    method(D,T,C,C,load,(const char *))             \
    var(D,T,C,List,fonts)
declare(Fonts, Base)

#define _Font(D,T,C) _Base(spr,T,C)                 \
    override(D,T,C,void,init,(C))                   \
    override(D,T,C,void,free,(C))                   \
    override(D,T,C,void,class_init,(Class))         \
    method(D,T,C,C,closest,(Gfx, char *, ushort))   \
    method(D,T,C,C,open,(Gfx, char *, ushort, List)) \
    method(D,T,C,bool,save_db,(Gfx, char *))        \
    method(D,T,C,bool,load_db,(Gfx, char *))        \
    var(D,T,C,String,family_name)                   \
    var(D,T,C,String,file_name)                     \
    var(D,T,C,ushort,point_size)                    \
    var(D,T,C,Surface,surface)                      \
    var(D,T,C,List,ranges)                          \
    var(D,T,C,GfxGlyphRange,ascii)                  \
    var(D,T,C,List,char_ranges)                     \
    var(D,T,C,int,n_glyphs)                         \
    var(D,T,C,int,ascent)                           \
    var(D,T,C,int,descent)                          \
    var(D,T,C,int,height)                           \
    var(D,T,C,bool,from_database)                   \
    var(D,T,C,uchar *,pixels)                       \
    var(D,T,C,struct _object_Font *,scaled)
declare(Font, Base)

#define _Glyph(D,T,C) _Base(spr,T,C)                \
    override(D,T,C,void,init,(C))                   \
	var(D,T,C,List,uv)                              \
	var(D,T,C,float,advance)                        \
    var(D,T,C,Vec2,offset)                          \
    var(D,T,C,Vec2,size)
declare(Glyph,Base)

#define _GlyphRange(D,T,C) _Base(spr,T,C)           \
	var(D,T,C,int,from)                             \
	var(D,T,C,int,to)                               \
    var(D,T,C,List,glyphs)
declare(GlyphRange,Base)

#define _CharRange(D,T,C) _Base(spr,T,C)            \
	private_var(D,T,C,int,from)                     \
	private_var(D,T,C,int,to)
declare(CharRange,Base)

#endif