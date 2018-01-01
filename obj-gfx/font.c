#include <obj-gfx/gfx.h>
#include <stdio.h>
#include <string.h>
#ifndef __EMSCRIPTEN__
#include <ft2build.h>
#include FT_FREETYPE_H
static FT_Library ft_library;
#endif

#define GFX_FONT_VERSION 2

implement(Fonts)

void Fonts_init(Fonts self) {
	self->fonts = new_list_of(List, Font);
}

void Fonts_free(Fonts self) {
	release(self->fonts);
}

bool Fonts_save(Fonts self, const char *file) {
	String json = call(self, to_json);
	if (!json)
		return false;
	return call(json, to_file, file);
}

Fonts Fonts_load(const char *file) {
	String json = class_call(String, from_file, file);
	return from_json(Fonts, json);
}

Font Fonts_find(Fonts self, char *family_name) {
	Font f;
	each(self->fonts, f) {
		if (strcmp(family_name, f->family_name) == 0)
			return f;
	}
	return NULL;
}

implement(Font)

void Font_class_init(Class c) {
#ifndef __EMSCRIPTEN__
	FT_Init_FreeType(&ft_library);
#endif
}

void Font_free(Font self) {
    release(self->surface);
    release(self->ranges);
	release(self->char_ranges);
	release(self->ranges);
	release(self->family_name);
	release(self->file_name);
	free(self->pixels);
}

bool Gfx_save_fonts(Gfx gfx, const char *file) {
	return call(gfx->fonts, save, file);
	// fonts needs to be a List class with an item_class set to class_object(Font); i.e. new_list_of(List, Font)
}

void Gfx_load_fonts(Gfx gfx, const char *file) {
	release(gfx->fonts);
	gfx->fonts = retain(class_call(Fonts, load, file));
}

bool Font_load_database(Gfx gfx, char *index_file) {
	return false;
}

Font Font_open(Gfx *gfx, char *font_face, u_short point_size) {
	Font augment = NULL;
	char *family_name = font_face;
#ifndef __EMSCRIPTEN__
	char *file_name = NULL;
	FT_Face ft_face = NULL;
	if (strstr(font_face, ".ttf")) {
		FT_New_Face(ft_library, font_face, 0, &ft_face);
		file_name = font_face;
		if (ft_face && ft_face->family_name)
			family_name = ft_face->family_name;
		else
			return NULL;
	}
#endif
	Font font = call(gfx->fonts, find, family_name);
#ifdef __EMSCRIPTEN__
        if (font)
            ++font->n_users;
#else
	if (font) {
		font->n_users++;
		if (ft_face)
			FT_Done_Face(ft_face);
		return font;
	} else if (!ft_face)
		return NULL;
#endif
#ifndef __EMSCRIPTEN__
	if (!augment) {
		font = (Font )malloc(sizeof(Font));
		memset(font, 0, sizeof(Font));
		font->n_users = 1;
	} else {
		font = augment;
		int n_users = font->n_users;
		gfx_font_free_contents(gfx, font);
		font->n_users = 1 + n_users;
	}
	font->family_name = copy_string(family_name);
	font->file_name = copy_string(file_name);
	font->point_size = point_size;
	if (ranges) {
		int size = sizeof(GfxCharRange) * range_count;
		font->char_ranges = (GfxCharRange *)malloc(size);
		memcpy(font->char_ranges, ranges, size);
	}
	font->n_ranges = range_count;
	font->ranges = (GfxGlyphRange *)malloc(sizeof(GfxGlyphRange) * range_count);
	font->ascii = &font->ranges[0];
	memset(font->ranges, 0, sizeof(GfxGlyphRange) * range_count);
	font->n_glyphs = 0;
	for (int r = 0; r < range_count; r++) {
		GfxGlyphRange *glyph_range = &font->ranges[r];
		glyph_range->from = min(ranges[r].to, ranges[r].from);
		glyph_range->to = max(ranges[r].to, ranges[r].from);
		glyph_range->n_glyphs = (glyph_range->to - glyph_range->from) + 1;
		glyph_range->glyphs = (GfxGlyph *)malloc(glyph_range->n_glyphs * sizeof(GfxGlyph));
		memset(glyph_range->glyphs, 0, glyph_range->n_glyphs * sizeof(GfxGlyph));
		font->n_glyphs += glyph_range->n_glyphs;
	}

	FT_Set_Char_Size(ft_face, 0, point_size << 6, 0, 0);
	FT_Size_Metrics *size = &ft_face->size->metrics;
	
	font->height = size->height >> 6;
	font->ascent = size->ascender >> 6;
	font->descent = abs((int)size->descender) >> 6;
	const int pad = 3;
	int max_dim = (pad + (ft_face->size->metrics.height >> 6)) * ceilf(sqrtf(font->n_glyphs)) + pad;
	int tex_width = 1;
	while(tex_width < max_dim) tex_width <<= 1;
	int tex_height = tex_width;
	u_char *pixels = (u_char *)malloc(tex_width * tex_height);
	int pen_x = pad, pen_y = pad;
	font->pixels = pixels;
	for (int r = 0; r < range_count; r++) {
		GfxGlyphRange *glyph_range = &font->ranges[r];
		for(int i = 0; i < glyph_range->n_glyphs; ++i) {
			int char_index = glyph_range->from + i;
			FT_Load_Char(ft_face, char_index, FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT);
			FT_Bitmap* bmp = &ft_face->glyph->bitmap;
			GfxGlyph *g = &glyph_range->glyphs[i];

			if (pen_x + bmp->width >= tex_width) {
				pen_x = pad;
				pen_y += ((ft_face->size->metrics.height >> 6) + pad);
			}
			for (int row = 0; row < bmp->rows; ++row) {
				for (int col = 0; col < bmp->width; ++col) {
					int x = pen_x + col;
					int y = pen_y + row;
					pixels[y * tex_width + x] = bmp->buffer[row * bmp->pitch + col];
				}
			}
			float left = (float)pen_x / (float)tex_width;
			float top = (float)pen_y / (float)tex_height;
			float right = (float)(pen_x + bmp->width) / (float)tex_width;
			float bot = (float)(pen_y + bmp->rows) / (float)tex_height;
			int ii = 0;
			g->uv[ii++] = left;  g->uv[ii++] = top;	// 00
			g->uv[ii++] = right; g->uv[ii++] = top;	// 10
			g->uv[ii++] = right; g->uv[ii++] = bot;	// 11
			g->uv[ii++] = left;  g->uv[ii++] = top;	// 00
			g->uv[ii++] = right; g->uv[ii++] = bot;	// 11
			g->uv[ii++] = left;  g->uv[ii++] = bot;	// 01
			g->w = bmp->width;
			g->h = bmp->rows;
			g->x_off = ft_face->glyph->bitmap_left;
			g->y_off = ft_face->glyph->bitmap_top;
			g->advance = ft_face->glyph->advance.x >> 6;
			pen_x += bmp->width + pad;
		}
	}
	font->surface = class_call(Surface, new_gray, gfx, tex_width, tex_height, pixels, tex_width, true);
	call(font->surface, texture_clamp, false);
	list_push(gfx->fonts, font);
	FT_Done_Face(ft_face);
#endif
	return font;
}

void Gfx_font_select(Gfx gfx, Font font) {
	gfx->state.font = font;
}

void Gfx_text_color(Gfx gfx, float r, float g, float b, float a) {
	gfx->state.text_color = (Color) { r, g, b, a };
}

// todo: implement UTF-8 decoding
void Gfx_text_scan(Gfx gfx, Font font, char *text, int len, void *arg, void(*pf_callback)(Gfx *, GfxGlyph *, void *, int)) {
	if (!text || !font)
		return;
	for (int i = 0; i < len; i++) {
		int ig = text[i];
		GfxGlyphRange *range = NULL;
		for (int r = 0; r < font->n_ranges; r++) {
			GfxGlyphRange *gr = &font->ranges[r];
			if (ig >= gr->from && ig <= gr->to) {
				range = gr;
				break;
			}
		}
		if (!range) {
			ig = 32;
			range = font->ascii;
			if (!range)
				continue;
		}
		int glyph_index = ig - range->from;
		if (glyph_index < 0 || glyph_index >= range->n_glyphs)
			continue;
		GfxGlyph *g = &range->glyphs[glyph_index];
		pf_callback(gfx, g, arg, i);
	}
}

typedef struct _GfxMeasureTextArgs {
	float x, y;
} GfxMeasureTextArgs;

void Gfx_measure_glyph(Gfx gfx, GfxGlyph *g, void *v_args, int str_index) {
	GfxMeasureTextArgs *args = v_args;
	args->x += g->advance + gfx->state.letter_spacing;
}

void Gfx_text_extents(Gfx gfx, char *text, int length, GfxTextExtents *ext) {
	if (!text)
		return;
	int len = length == -1 ? strlen(text) : length;
	Font font = gfx->state.font;
	if (!font)
		return;
	GfxMeasureTextArgs args = { .x = 0 };
	gfx_text_scan(gfx, font, text, len, &args, gfx_measure_glyph);
	if (args.x > 0.0)
		args.x -= gfx->state.letter_spacing;
	ext->w = args.x;
	ext->h = (float)(font->ascent + font->descent) * gfx->state.line_scale;
	ext->ascent = font->ascent;
	ext->descent = font->descent;
}

typedef struct _GfxDrawTextArgs {
	float x, y;
	VertexText *v;
	Color *palette;
	u_char *colors;
} GfxDrawTextArgs;

void Gfx_draw_glyph(Gfx gfx, Glyph g, void *v_args, int str_index) {
	GfxDrawTextArgs *args = v_args;
	Color c = args->palette ? args->palette[args->colors[str_index]] : gfx->state.text_color;
	float ox = args->x + g->x_off, oy = args->y - g->y_off;
	float pos[12] = {
		ox, oy,
		ox + g->w, oy,
		ox + g->w, oy + g->h,
		ox, oy,
		ox + g->w, oy + g->h,
		ox, oy + g->h
	};
	for (int iv = 0; iv < 12; iv += 2, args->v++) {
		Vec2 p = (Vec2) { pos[iv], pos[iv + 1] };
		args->v->pos = p;
		args->v->u = g->uv[iv];
		args->v->v = g->uv[iv + 1];
		args->v->color = c;
	}
	args->x += g->advance + gfx->state.letter_spacing;
}
void gfx_draw_text(Gfx *gfx, char *text, int length, Color *palette, u_char *colors) {
	if (!text)
		return;
	int len = length == -1 ? strlen(text) : length;
	Font font = gfx->state.font;
	if (!font)
		return;
	Font font_prev = font;
	float sx = 0, sy = 0;
	float allowance = 0.25;
	call(gfx, get_scales, &sx, &sy);
	float s = (sx + sy) / 2.0;
	bool perform_scale = false;
	bool up_scale = fabs(s - 1.0) > allowance;
	bool second_scale = false;
	if (up_scale || gfx->state.arb_rotation) {
		if (!up_scale) {
			s = max(2.0, s);
			call(gfx, push);
			call(gfx, scale, 1.0 / s, 1.0 / s);
			second_scale = true;
		}
		float scaled_size = (float)font->point_size * s;
		Font scaled_font = NULL;
		if (font->scaled && font->scaled->point_size == (u_short)scaled_size)
			scaled_font = font->scaled;
		if (!scaled_font) {
			int range_count = font->n_ranges;
			Font augment;
			scaled_font = gfx_font_find(gfx, font->family_name, scaled_size,
				&font->char_ranges, &range_count, &augment);
		}
		if (!scaled_font) {
			if (font->scaled) {
				release(font->scaled);
				font->scaled = NULL;
			}
			scaled_font = class_call(Font, open, gfx,
				font->file_name ? font->file_name : font->family_name,
				scaled_size, font->char_ranges, font->n_ranges);
		}
		if (scaled_font) {
			font->scaled = scaled_font;
			font = scaled_font;
			perform_scale = true;
			call(gfx, push);
			gfx_scale(gfx, 1.0 / sx, 1.0 / sy);
		}
	}
	gfx->state.font = font;
	// find scale in matrix
	int n_verts = len * 6;
	gfx_realloc_buffer(gfx, n_verts);
	GfxDrawTextArgs args = {
		.palette = palette, .colors = colors,
		.x = 0, .y = 0,
		.v = (VertexText *)gfx->vbuffer
	};
	gfx_text_scan(gfx, font, text, len, &args, gfx_draw_glyph);
	glBindBuffer(GL_ARRAY_BUFFER, gfx->vbo);
	glBufferData(GL_ARRAY_BUFFER, n_verts * sizeof(VertexText), gfx->vbuffer, GL_STATIC_DRAW);
	call(gfx, shaders_use, SHADER_TEXT, NULL, false);
	GfxClip *clip = ll_last(&gfx->clips);
	gfx_clip_surface(gfx, SHADER_TEXT, clip);
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gfx_surface_clamp(gfx, font->surface, true);
	glDrawArrays(GL_TRIANGLES, 0, n_verts);
	gfx->state.font = font_prev;
	if (perform_scale)
		call(gfx, pop);
	if (second_scale)
		call(gfx, pop);
}

void gfx_text_ellipsis(Gfx *gfx, char *text, int len, char *output, int max_w, GfxTextExtents *pext) {
    const char *ellipsis = "...";
    if (len == -1)
        len = strlen(text);
    char *buf = malloc(len + 4);
    for (int i = len; i >= 0; i--) {
        memcpy(buf, text, i);
        buf[i] = 0;
        int slen = i;
        if (i < len) {
            memcpy(&buf[i], ellipsis, 3);
            slen = i + 3;
            buf[slen] = 0;
        }
        gfx_text_extents(gfx, buf, slen, pext);
        if (pext->w < max_w) {
            memcpy(output, buf, slen);
            output[slen] = 0;
            return;
        }
    }
}

void gfx_draw_text_ellipsis(Gfx *gfx, char *text, int len, int max_w) {
    GfxTextExtents ext;
    char buf[len + 32];
    buf[0] = 0;
    gfx_text_ellipsis(gfx, text, len, buf, max_w, &ext);
    gfx_draw_text(gfx, buf, -1, NULL, NULL);
}

implement(CharRange)

static Pairs unicode_ranges;

void CharRange_class_init(Class cself) {
	class_call(CharRange, new_range, 0x0020, 0x007F, "Basic Latin");
	class_call(CharRange, new_range, 0x2580, 0x259F, "Block Elements");
	class_call(CharRange, new_range, 0x00A0, 0x00FF, "Latin-1 Supplement");
	class_call(CharRange, new_range, 0x25A0, 0x25FF, "Geometric Shapes");
	class_call(CharRange, new_range, 0x0100, 0x017F, "Latin Extended-A");
	class_call(CharRange, new_range, 0x2600, 0x26FF, "Miscellaneous Symbols");
	class_call(CharRange, new_range, 0x0180, 0x024F, "Latin Extended-B");
	class_call(CharRange, new_range, 0x2700, 0x27BF, "Dingbats");
	class_call(CharRange, new_range, 0x0250, 0x02AF, "IPA Extensions");
	class_call(CharRange, new_range, 0x27C0, 0x27EF, "Miscellaneous Mathematical Symbols-A");
	class_call(CharRange, new_range, 0x02B0, 0x02FF, "Spacing Modifier Letters");
	class_call(CharRange, new_range, 0x27F0, 0x27FF, "Supplemental Arrows-A");
	class_call(CharRange, new_range, 0x0300, 0x036F, "Combining Diacritical Marks");
	class_call(CharRange, new_range, 0x2800, 0x28FF, "Braille Patterns");
	class_call(CharRange, new_range, 0x0370, 0x03FF, "Greek and Coptic");
	class_call(CharRange, new_range, 0x2900, 0x297F, "Supplemental Arrows-B");
	class_call(CharRange, new_range, 0x0400, 0x04FF, "Cyrillic");
	class_call(CharRange, new_range, 0x2980, 0x29FF, "Miscellaneous Mathematical Symbols-B");
	class_call(CharRange, new_range, 0x0500, 0x052F, "Cyrillic Supplementary");
	class_call(CharRange, new_range, 0x2A00, 0x2AFF, "Supplemental Mathematical Operators");
	class_call(CharRange, new_range, 0x0530, 0x058F, "Armenian");
	class_call(CharRange, new_range, 0x2B00, 0x2BFF, "Miscellaneous Symbols and Arrows");
	class_call(CharRange, new_range, 0x0590, 0x05FF, "Hebrew");
	class_call(CharRange, new_range, 0x2E80, 0x2EFF, "CJK Radicals Supplement");
	class_call(CharRange, new_range, 0x0600, 0x06FF, "Arabic");
	class_call(CharRange, new_range, 0x2F00, 0x2FDF, "Kangxi Radicals");
	class_call(CharRange, new_range, 0x0700, 0x074F, "Syriac");
	class_call(CharRange, new_range, 0xFF0, 0x2FFF, "Ideographic Description Characters");
	class_call(CharRange, new_range, 0x0780, 0x07BF, "Thaana");
	class_call(CharRange, new_range, 0x3000, 0x303F, "CJK Symbols and Punctuation");
	class_call(CharRange, new_range, 0x0900, 0x097F, "Devanagari");
	class_call(CharRange, new_range, 0x3040, 0x309F, "Hiragana");
	class_call(CharRange, new_range, 0x0980, 0x09FF, "Bengali");
	class_call(CharRange, new_range, 0x30A0, 0x30FF, "Katakana");
	class_call(CharRange, new_range, 0x0A00, 0x0A7F, "Gurmukhi");
	class_call(CharRange, new_range, 0x3100, 0x312F, "Bopomofo");
	class_call(CharRange, new_range, 0x0A80, 0x0AFF, "Gujarati");
	class_call(CharRange, new_range, 0x3130, 0x318F, "Hangul Compatibility Jamo");
	class_call(CharRange, new_range, 0x0B00, 0x0B7F, "Oriya");
	class_call(CharRange, new_range, 0x3190, 0x319F, "Kanbun");
	class_call(CharRange, new_range, 0x0B80, 0x0BFF, "Tamil");
	class_call(CharRange, new_range, 0x31A0, 0x31BF, "Bopomofo Extended");
	class_call(CharRange, new_range, 0x0C00, 0x0C7F, "Telugu");
	class_call(CharRange, new_range, 0x31F0, 0x31FF, "Katakana Phonetic Extensions");
	class_call(CharRange, new_range, 0x0C80, 0x0CFF, "Kannada");
	class_call(CharRange, new_range, 0x3200, 0x32FF, "Enclosed CJK Letters and Months");
	class_call(CharRange, new_range, 0x0D00, 0x0D7F, "Malayalam");
	class_call(CharRange, new_range, 0x3300, 0x33FF, "CJK Compatibility");
	class_call(CharRange, new_range, 0x0D80, 0x0DFF, "Sinhala");
	class_call(CharRange, new_range, 0x3400, 0x4DBF, "CJK Unified Ideographs Extension A");
	class_call(CharRange, new_range, 0x0E00, 0x0E7F, "Thai");
	class_call(CharRange, new_range, 0x4DC0, 0x4DFF, "Yijing Hexagram Symbols");
	class_call(CharRange, new_range, 0x0E80, 0x0EFF, "Lao");
	class_call(CharRange, new_range, 0x4E00, 0x9FFF, "CJK Unified Ideographs");
	class_call(CharRange, new_range, 0x0F00, 0x0FFF, "Tibetan");
	class_call(CharRange, new_range, 0xA000, 0xA48F, "Yi Syllables");
	class_call(CharRange, new_range, 0x1000, 0x109F, "Myanmar");
	class_call(CharRange, new_range, 0xA490, 0xA4CF, "Yi Radicals");
	class_call(CharRange, new_range, 0x10A0, 0x10FF, "Georgian");
	class_call(CharRange, new_range, 0xAC00, 0xD7AF, "Hangul Syllables");
	class_call(CharRange, new_range, 0x1100, 0x11FF, "Hangul Jamo");
	class_call(CharRange, new_range, 0xD800, 0xDB7F, "High Surrogates");
	class_call(CharRange, new_range, 0x1200, 0x137F, "Ethiopic");
	class_call(CharRange, new_range, 0xDB80, 0xDBFF, "High Private Use Surrogates");
	class_call(CharRange, new_range, 0x13A0, 0x13FF, "Cherokee");
	class_call(CharRange, new_range, 0xDC00, 0xDFFF, "Low Surrogates");
	class_call(CharRange, new_range, 0x1400, 0x167F, "Unified Canadian Aboriginal Syllabics");
	class_call(CharRange, new_range, 0xE000, 0xF8FF, "Private Use Area");
	class_call(CharRange, new_range, 0x1680, 0x169F, "Ogham");
	class_call(CharRange, new_range, 0xF900, 0xFAFF, "CJK Compatibility Ideographs");
	class_call(CharRange, new_range, 0x16A0, 0x16FF, "Runic");
	class_call(CharRange, new_range, 0xFB00, 0xFB4F, "Alphabetic Presentation Forms");
	class_call(CharRange, new_range, 0x1700, 0x171F, "Tagalog");
	class_call(CharRange, new_range, 0xFB50, 0xFDFF, "Arabic Presentation Forms-A");
	class_call(CharRange, new_range, 0x1720, 0x173F, "Hanunoo");
	class_call(CharRange, new_range, 0xFE00, 0xFE0F, "Variation Selectors");
	class_call(CharRange, new_range, 0x1740, 0x175F, "Buhid");
	class_call(CharRange, new_range, 0xFE20, 0xFE2F, "Combining Half Marks");
	class_call(CharRange, new_range, 0x1760, 0x177F, "Tagbanwa");
	class_call(CharRange, new_range, 0xFE30, 0xFE4F, "CJK Compatibility Forms");
	class_call(CharRange, new_range, 0x1780, 0x17FF, "Khmer");
	class_call(CharRange, new_range, 0xFE50, 0xFE6F, "Small Form Variants");
	class_call(CharRange, new_range, 0x1800, 0x18AF, "Mongolian");
	class_call(CharRange, new_range, 0xFE70, 0xFEFF, "Arabic Presentation Forms-B");
	class_call(CharRange, new_range, 0x1900, 0x194F, "Limbu");
	class_call(CharRange, new_range, 0xFF00, 0xFFEF, "Halfwidth and Fullwidth Forms");
	class_call(CharRange, new_range, 0x1950, 0x197F, "Tai Le");
	class_call(CharRange, new_range, 0xFFF0, 0xFFFF, "Specials");
	class_call(CharRange, new_range, 0x19E0, 0x19FF, "Khmer Symbols");
	class_call(CharRange, new_range, 0x10000, 0x1007F, "Linear B Syllabary");
	class_call(CharRange, new_range, 0x1D00, 0x1D7F, "Phonetic Extensions");
	class_call(CharRange, new_range, 0x10080, 0x100FF, "Linear B Ideograms");
	class_call(CharRange, new_range, 0x1E00, 0x1EFF, "Latin Extended Additional");
	class_call(CharRange, new_range, 0x10100, 0x1013F, "Aegean Numbers");
	class_call(CharRange, new_range, 0x1F00, 0x1FFF, "Greek Extended");
	class_call(CharRange, new_range, 0x10300, 0x1032F, "Old Italic");
	class_call(CharRange, new_range, 0x2000, 0x206F, "General Punctuation");
	class_call(CharRange, new_range, 0x10330, 0x1034F, "Gothic");
	class_call(CharRange, new_range, 0x2070, 0x209F, "Superscripts and Subscripts");
	class_call(CharRange, new_range, 0x10380, 0x1039F, "Ugaritic");
	class_call(CharRange, new_range, 0x20A0, 0x20CF, "Currency Symbols");
	class_call(CharRange, new_range, 0x10400, 0x1044F, "Deseret");
	class_call(CharRange, new_range, 0x20D0, 0x20FF, "Combining Diacritical Marks for Symbols");
	class_call(CharRange, new_range, 0x10450, 0x1047F, "Shavian");
	class_call(CharRange, new_range, 0x2100, 0x214F, "Letterlike Symbols");
	class_call(CharRange, new_range, 0x10480, 0x104AF, "Osmanya");
	class_call(CharRange, new_range, 0x2150, 0x218F, "Number Forms");
	class_call(CharRange, new_range, 0x10800, 0x1083F, "Cypriot Syllabary");
	class_call(CharRange, new_range, 0x2190, 0x21FF, "Arrows");
	class_call(CharRange, new_range, 0x1D000, 0x1D0FF, "Byzantine Musical Symbols");
	class_call(CharRange, new_range, 0x2200, 0x22FF, "Mathematical Operators");
	class_call(CharRange, new_range, 0x1D100, 0x1D1FF, "Musical Symbols");
	class_call(CharRange, new_range, 0x2300, 0x23FF, "Miscellaneous Technical");
	class_call(CharRange, new_range, 0x1D300, 0x1D35F, "Tai Xuan Jing Symbols");
	class_call(CharRange, new_range, 0x2400, 0x243F, "Control Pictures");
	class_call(CharRange, new_range, 0x1D400, 0x1D7FF, "Mathematical Alphanumeric Symbols");
	class_call(CharRange, new_range, 0x2440, 0x245F, "Optical Character Recognition");
	class_call(CharRange, new_range, 0x20000, 0x2A6DF, "CJK Unified Ideographs Extension B");
	class_call(CharRange, new_range, 0x2460, 0x24FF, "Enclosed Alphanumerics");
	class_call(CharRange, new_range, 0x2F800, 0x2FA1F, "CJK Compatibility Ideographs Supplement");
	class_call(CharRange, new_range, 0x2500, 0x257F, "Box Drawing");
	class_call(CharRange, new_range, 0xE0000, 0xE007F, "Tags");
}

void CharRange_new_range(int from, int to, const char *name) {
	CharRange self = new(CharRange);
	self->from = from;
	self->to = to;
	self->name = new_string(name);
	if (!unicode_ranges)
		unicode_ranges = new(Pairs);
	pairs_add(unicode_ranges, new_string(name), self);
	return self;
}

void CharRange_from_name(const char *name) {
	String str_name = string(name);
	CharRange self = pairs_value(unicode_ranges, str_name, CharRange);
	return self;
}

implement(GlyphRange)

implement(Glyph)

void Glyph_init(Glyph self) {
	self->uv = new_list_of(List, Vec2);
}

void Glyph_free(Glyph self) {
	release(self->uv);
}

