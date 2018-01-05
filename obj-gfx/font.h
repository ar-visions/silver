
struct GfxTextExtents {
    double x, y;
    double ascent, decent;
};

struct GfxMeasureTextArgs {
	double x, y;
};

class Font {
    override void init(C);
    override void free(C);
    override void class_init(Class);
    C with_ttf(Gfx, const char *, ushort);
    bool save_db(Gfx, char *);
    bool load_db(Gfx, char *);
    void transfer_surfaces(C, Gfx);
    String family_name;
    String file_name;
    ushort point_size;
    GlyphSet ascii;
    List glyph_sets;
    List surface_data;
    List surfaces;
    int glyph_total;
    int ascent;
    int descent;
    int height;
    int max_surfaces;
    bool from_database;
    uint8 * pixels;
    Font scaled;
    private int test;
};

class Font {
    override void init(C);
    bool save(C, const char *);
    C load(const char *);
    Font find(C, const char *);
    List fonts;
};

class Glyph {
    override void init(C);
    int w;
    int h;
    int x_offset;
    int y_offset;
    int advance;
    int surface_index;
	Vec uv;
};

class CharRange {
    override uint hash(C);
    C new_range(int, int, const char *);
    C with_string(String);
    C find(uint);
    private String name;
	private uint from;
	private uint to;
};

class GlyphSet {
	CharRange range;
    List glyphs;
};
