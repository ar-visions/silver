#include <obj-gfx/gfx.h>
#include <stdio.h>
#include <string.h>
#ifndef __EMSCRIPTEN__
#include <ft2build.h>
#include FT_FREETYPE_H
static FT_Library ft_library;
#endif

#define GFX_FONT_VERSION 2

void GfxFont_class_init(Class c) {
#ifndef __EMSCRIPTEN__
	FT_Init_FreeType(&ft_library);
#endif
}

void GfxFont_free(GfxFont self) {
    release(self->surface);
    release(self->ranges);
	release(font->char_ranges);
	release(font->ranges);
	release(font->family_name);
	release(font->file_name);
	free(font->pixels);
}

BOOL Gfx_save_fonts(Gfx self, char *index_file) {
	if (list_count(self->fonts) == 0)
		return false;
    GfxFont font;
	each(self->fonts, font) {
		if (!font->pixels)
			return false;
	}
	FILE *f = fopen(index_file, "w");
	if (!f)
		return false;
	u_char ver = GFX_FONT_VERSION;
	fwrite(&ver, sizeof(u_char), 1, f);
	u_char count = list_count(self->fonts);
	fwrite(&count, sizeof(u_char), 1, f);
	each(&gfx->fonts, font) {
		u_char face_len = strlen(font->family_name);
		fwrite(&face_len, sizeof(u_char), 1, f);
		fwrite(font->family_name, face_len, 1, f);
		fwrite(&font->point_size, sizeof(u_short), 1, f);
		u_char n_ranges = font->n_ranges;
		fwrite(&n_ranges, sizeof(u_char), 1, f);
		for (int i = 0; i < n_ranges; i++) {
			fwrite(&font->ranges[i].from, sizeof(int), 1, f);
			fwrite(&font->ranges[i].to, sizeof(int), 1, f);
			int n_glyphs = font->ranges[i].to - font->ranges[i].from + 1;
			fwrite(font->ranges[i].glyphs, sizeof(GfxGlyph) * n_glyphs, 1, f);
		}
		GfxSurface *s = font->surface;
		fwrite(&s->w, sizeof(int), 1, f);
		fwrite(&s->h, sizeof(int), 1, f);
		if (!font->pixels) {
			return FALSE;
		}
		fwrite(font->pixels, sizeof(u_char) * s->w * s->h, 1, f);
	}
	fclose(f);
	return TRUE;
}

BOOL gfx_font_load_database(Gfx *gfx, char *index_file) {
	FILE *f = fopen(index_file, "r");
	if (!f)
		return FALSE;
	u_char ver = 0;
	if (fread(&ver, sizeof(u_char), 1, f) != 1)
		return FALSE;
	if (ver != 1)
		return FALSE;
	u_char count = 0;
	if (fread(&count, sizeof(u_char), 1, f) != 1)
		return FALSE;
	BOOL err = FALSE;
	for (int i = 0; i < count; i++) {
		u_char face_len = 0;
		if (fread(&face_len, sizeof(u_char), 1, f) != 1) {
			err = TRUE;
			break;
		}
		GfxFont *font = (GfxFont *)malloc(sizeof(GfxFont));
		memset(font, 0, sizeof(GfxFont));
		font->n_users = 1;
		font->family_name = (char *)malloc(face_len + 1);
		if (fread(font->family_name, face_len, 1, f) != 1) { err = TRUE; break; }
		font->family_name[face_len] = 0;
		if (fread(&font->point_size, sizeof(u_short), 1, f) != 1) { err = TRUE; break; }
		u_char n_ranges = 0;
		if (fread(&n_ranges, sizeof(u_char), 1, f) != 1) { err = TRUE; break; }
		font->n_ranges = n_ranges;
		font->ranges = (GfxGlyphRange *)malloc(sizeof(GfxGlyphRange) * n_ranges);
		memset(font->ranges, 0, sizeof(GfxGlyphRange) * n_ranges);
		font->ascii = &font->ranges[0];
		for (int i = 0; i < n_ranges; i++) {
			GfxGlyphRange *range = &font->ranges[i];
			if (fread(&range->from, sizeof(int), 1, f) != 1) { err = TRUE; break; }
			if (fread(&range->to, sizeof(int), 1, f) != 1) { err = TRUE; break; }
			range->n_glyphs = range->to - range->from + 1;
			range->glyphs = (GfxGlyph *)malloc(sizeof(GfxGlyph) * range->n_glyphs);
			memset(range->glyphs, 0, sizeof(GfxGlyph) * range->n_glyphs);
			if (fread(range->glyphs, sizeof(GfxGlyph) * range->n_glyphs, 1, f) != 1) { err = TRUE; break; }
			font->n_glyphs += range->n_glyphs;
		}
		if (err) break;
		int w = 0, h = 0;
		if (fread(&w, sizeof(int), 1, f) != 1) { err = TRUE; break; }
		if (fread(&h, sizeof(int), 1, f) != 1) { err = TRUE; break; }
		u_char *pixels = (u_char *)malloc(sizeof(u_char) * w * h);
		if (fread(pixels, sizeof(u_char) * w * h, 1, f) != 1) { err = TRUE; break; }
		font->surface = gfx_surface_create_gray(gfx, w, h, pixels, w, TRUE);
		font->from_database = TRUE;
		ll_push(&gfx->fonts, font);
	}
	fclose(f);
	if (err) {
		ll_each(&gfx->fonts, GfxFont, font) {
			if (!font->surface) {
				font->from_database = FALSE;
				gfx_font_destroy(gfx, font);
			}
		}
	}
	return !err;
}

static GfxCharRange ansi_range = { 0, 255 };

static int
overlap_amount(int a_0, int a_1, int b_0, int b_1) {
    if (a_1 < b_0 || a_0 > b_1)
        return 0;
    int il = max(a_0, b_0);
    int ir = min(a_1, b_1);
    return ir - il + 1;
}

static
GfxFont *gfx_font_find(Gfx *gfx, char *family_name, u_short point_size, GfxCharRange **ranges, int *range_count, GfxFont **augment) {
	if (!(*ranges)) {
		*ranges = &ansi_range;
		if (*range_count >= 128)
			(*ranges)->to = *range_count - 1;
		*range_count = 1;
	} else if ((*ranges)[0].from < 0 || (*ranges)[0].to < 127) {
		(*ranges)[0].from = 0;
		(*ranges)[0].to = max((*ranges)[0].to, 127);
	}
	int n_glyphs = 0;
	int count = *range_count;
	GfxCharRange *r = *ranges;
	for (int i = 0; i < count; i++) {
		n_glyphs += (r[i].to - r[i].from) + 1;
	}
	ll_each(&gfx->fonts, GfxFont, f) {
		if (strcmp(family_name, f->family_name) == 0 && f->point_size == point_size) {
			GfxCharRange *r = *ranges;
			// check if full range is contained in font
			int overlap_count = 0;
			for (int i = 0; i < count; i++) {
				for (int ii = 0; ii < f->n_ranges; ii++) {
					GfxGlyphRange *gr = &f->ranges[ii];
					overlap_count += overlap_amount(r[i].from, r[i].to, gr->from, gr->to);
				}
			}
			if (overlap_count < n_glyphs) {
				*augment = f;
				continue;
			}
			*augment = NULL;
			return f;
		}
	}
	return NULL;
}

GfxFont *gfx_font_closest(Gfx *gfx, char *family_name, u_short point_size) {
	BOOL face_found = FALSE;
	ll_each(&gfx->fonts, GfxFont, f) {
		if (strcmp(family_name, f->family_name) == 0) {
			face_found = TRUE;
			if (f->point_size == point_size)
				return f;
		}
	}
	GfxFont *closest = NULL;
	ll_each(&gfx->fonts, GfxFont, ff) {
		if (face_found && strcmp(family_name, ff->family_name) != 0)
			continue;
		if (!closest || abs(closest->point_size - point_size) > abs(ff->point_size - point_size))
			closest = ff;
	}
	return closest;
}

GfxFont *gfx_font_open(Gfx *gfx, char *font_face, u_short point_size, GfxCharRange *ranges, int range_count) {
	GfxFont *augment = NULL;
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
	GfxFont *font = gfx_font_find(gfx, family_name, point_size, &ranges, &range_count, &augment);
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
		font = (GfxFont *)malloc(sizeof(GfxFont));
		memset(font, 0, sizeof(GfxFont));
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
	font->surface = gfx_surface_create_gray(gfx, tex_width, tex_height, pixels, tex_width, TRUE);
	gfx_surface_clamp(gfx, font->surface, FALSE);
	ll_push(&gfx->fonts, font);
	FT_Done_Face(ft_face);
#endif
	return font;
}

void gfx_font_select(Gfx *gfx, GfxFont *font) {
	gfx->state.font = font;
}

void gfx_text_color(Gfx *gfx, float r, float g, float b, float a) {
	gfx->state.text_color = (Color) { r, g, b, a };
}

// todo: implement UTF-8 decoding
void gfx_text_scan(Gfx *gfx, GfxFont *font, char *text, int len, void *arg, void(*pf_callback)(Gfx *, GfxGlyph *, void *, int)) {
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

void gfx_measure_glyph(Gfx *gfx, GfxGlyph *g, void *v_args, int str_index) {
	GfxMeasureTextArgs *args = v_args;
	args->x += g->advance + gfx->state.letter_spacing;
}

void gfx_text_extents(Gfx *gfx, char *text, int length, GfxTextExtents *ext) {
	if (!text)
		return;
	int len = length == -1 ? strlen(text) : length;
	GfxFont *font = gfx->state.font;
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

void gfx_draw_glyph(Gfx *gfx, GfxGlyph *g, void *v_args, int str_index) {
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
	GfxFont *font = gfx->state.font;
	if (!font)
		return;
	GfxFont *font_prev = font;
	float sx = 0, sy = 0;
	float allowance = 0.25;
	gfx_get_scales(gfx, &sx, &sy);
	float s = (sx + sy) / 2.0;
	BOOL perform_scale = FALSE;
	BOOL up_scale = fabs(s - 1.0) > allowance;
	BOOL second_scale = FALSE;
	if (up_scale || gfx->state.arb_rotation) {
		if (!up_scale) {
			s = max(2.0, s);
			gfx_push(gfx);
			gfx_scale(gfx, 1.0 / s, 1.0 / s);
			second_scale = TRUE;
		}
		float scaled_size = (float)font->point_size * s;
		GfxFont *scaled_font = NULL;
		if (font->scaled && font->scaled->point_size == (u_short)scaled_size)
			scaled_font = font->scaled;
		if (!scaled_font) {
			int range_count = font->n_ranges;
			GfxFont *augment;
			scaled_font = gfx_font_find(gfx, font->family_name, scaled_size,
				&font->char_ranges, &range_count, &augment);
		}
		if (!scaled_font) {
			if (font->scaled) {
				gfx_font_destroy(gfx, font->scaled);
				font->scaled = NULL;
			}
			scaled_font = gfx_font_open(gfx,
				font->file_name ? font->file_name : font->family_name,
				scaled_size, font->char_ranges, font->n_ranges);
		}
		if (scaled_font) {
			font->scaled = scaled_font;
			font = scaled_font;
			perform_scale = TRUE;
			gfx_push(gfx);
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
	gfx_shaders_use(gfx, SHADER_TEXT, NULL, FALSE);
	GfxClip *clip = ll_last(&gfx->clips);
	gfx_clip_surface(gfx, SHADER_TEXT, clip);
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gfx_surface_clamp(gfx, font->surface, TRUE);
	glDrawArrays(GL_TRIANGLES, 0, n_verts);
	gfx->state.font = font_prev;
	if (perform_scale)
		gfx_pop(gfx);
	if (second_scale)
		gfx_pop(gfx);
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
