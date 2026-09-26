/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

// silver's webgfx module: trinity does the work behind these
extern "C" {
int webgfx_font_open(u8 const* data, i64 size, int index);
void webgfx_font_close(int font);
int webgfx_font_glyph_count(int font);
int webgfx_font_units_per_em(int font);
u32 webgfx_font_glyph_index(int font, u32 code_point);
void webgfx_font_metrics(int font, float px, float* out);
void webgfx_font_outline(int font, u32 glyph, int path, float x, float y, float px);
int webgfx_font_family(int font, u8* buffer, int capacity);
void webgfx_font_style(int font, int* out);
void webgfx_font_set_axes(int font, u32 const* tags, float const* values, int count);
float webgfx_font_advance(int font, u32 glyph, float px);
bool webgfx_font_match(char const* family, int weight, int width, int slope, u32 code_point,
    bool emoji, u8* file, int file_capacity, u8* family_out, int family_capacity, int* index);

void webgfx_gpu_init();
int webgfx_canvas_new(int width, int height);
void webgfx_canvas_free(int canvas);
void webgfx_canvas_clear(int canvas, float const* rgba);
void webgfx_canvas_save(int canvas);
void webgfx_canvas_restore(int canvas);
void webgfx_canvas_transform(int canvas, float const* matrix);
void webgfx_canvas_clip(int canvas, int x, int y, int width, int height);
void webgfx_canvas_fill_rect(int canvas, float x, float y, float width, float height, float const* rgba, float const* radii);
void webgfx_canvas_fill_path(int canvas, int path, float const* rgba, bool even_odd);
void webgfx_canvas_stroke_path(int canvas, int path, float const* rgba, float width);
void webgfx_canvas_glyphs(int canvas, int font, float px, u32 const* ids, float const* xs, float const* ys, int count, float const* rgba);
void webgfx_canvas_image(int canvas, u8 const* rgba_pixels, int width, int height, float const* dst, float const* uv);
void webgfx_canvas_read(int canvas, u8* rgba_out);
int webgfx_image_new(u8 const* rgba_pixels, int width, int height);
void webgfx_image_update(int image, u8 const* rgba_pixels, int width, int height);
int webgfx_plane_new(int width, int height);
void webgfx_plane_update(int plane, u8 const* bytes, int width, int height);
void webgfx_canvas_draw_yuv(int canvas, int y, int u, int v, float const* dst, float const* rows);
void webgfx_image_free(int image);
void webgfx_canvas_draw_image(int canvas, int image, float const* dst, float const* uv);
void webgfx_canvas_draw_layer(int canvas, int layer, int mask, bool luminance, int x, int y, int width, int height);

int webgfx_path_new();
void webgfx_path_free(int path);
int webgfx_path_clone(int path);
void webgfx_path_move_to(int path, float x, float y);
void webgfx_path_line_to(int path, float x, float y);
void webgfx_path_quad_to(int path, float cx, float cy, float x, float y);
void webgfx_path_cubic_to(int path, float c1x, float c1y, float c2x, float c2y, float x, float y);
void webgfx_path_arc_to(int path, float x, float y, float rx, float ry, float rotation, bool large, bool sweep);
void webgfx_path_close(int path);
int webgfx_path_count(int path);
void webgfx_path_point(int path, int index, float* out);
void webgfx_path_cursor(int path, float* out);
void webgfx_path_bounds(int path, float* out);
bool webgfx_path_contains(int path, float x, float y, bool even_odd);
float webgfx_path_length(int path);
void webgfx_path_point_at(int path, float distance, float* out);
void webgfx_path_transform(int path, float const* matrix);
}

namespace Gfx::WebGfx {

// webgfx_path_point's first value
enum class PathCommand : u8 {
    Move = 0,
    Line = 1,
    Quad = 2,
    Cubic = 3,
};

}
