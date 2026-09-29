/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <cstdint>

// silver's webgfx module: trinity does the work behind these
extern "C" {
int webgfx_font_open(uint8_t const* data, int64_t size, int index);
void webgfx_font_close(int font);
int webgfx_font_glyph_count(int font);
int webgfx_font_units_per_em(int font);
uint32_t webgfx_font_glyph_index(int font, uint32_t code_point);
void webgfx_font_metrics(int font, float px, float* out);
void webgfx_font_outline(int font, uint32_t glyph, int path, float x, float y, float px);
int webgfx_font_family(int font, uint8_t* buffer, int capacity);
void webgfx_font_style(int font, int* out);
void webgfx_font_set_axes(int font, uint32_t const* tags, float const* values, int count);
float webgfx_font_advance(int font, uint32_t glyph, float px);
bool webgfx_font_match(char const* family, int weight, int width, int slope, uint32_t code_point,
    bool emoji, uint8_t* file, int file_capacity, uint8_t* family_out, int family_capacity, int* index);

void webgfx_gpu_init();
int webgfx_canvas_new(int width, int height);
void webgfx_canvas_free(int canvas);
void webgfx_drain();
void webgfx_canvas_clear(int canvas, float const* rgba);
void webgfx_canvas_save(int canvas);
void webgfx_canvas_restore(int canvas);
void webgfx_canvas_transform(int canvas, float const* matrix);
void webgfx_canvas_clip(int canvas, int x, int y, int width, int height);
void webgfx_canvas_fill_rect(int canvas, float x, float y, float width, float height, float const* rgba, float const* radii);
void webgfx_canvas_fill_path(int canvas, int path, float const* rgba, bool even_odd);
// centred on the path; cap: 0 butt, 1 round, 2 square
void webgfx_canvas_stroke_path(int canvas, int path, float const* rgba, float width, int cap);
void webgfx_canvas_glyphs(int canvas, int font, float px, uint32_t const* ids, float const* xs, float const* ys, int count, float const* rgba);
void webgfx_canvas_image(int canvas, uint8_t const* rgba_pixels, int width, int height, float const* dst, float const* uv);
void webgfx_canvas_read(int canvas, uint8_t* rgba_out);
void webgfx_canvas_read_region(int canvas, int x, int y, int width, int height, uint8_t* rgba_out);
int webgfx_image_new(uint8_t const* rgba_pixels, int width, int height);
void webgfx_image_update(int image, uint8_t const* rgba_pixels, int width, int height);
int webgfx_plane_new(int width, int height);
void webgfx_plane_update(int plane, uint8_t const* bytes, int width, int height);
void webgfx_canvas_draw_yuv(int canvas, int y, int u, int v, float const* dst, float const* rows);
void webgfx_image_free(int image);

// a whole mp4 held for playback: its h.264 decoded on the gpu
int webgfx_video_new(uint8_t const* bytes, int64_t size);
void webgfx_video_free(int video);
void webgfx_video_info(int video, int* width, int* height, double* duration);
// decode up to seconds; 1 when a newer frame is on screen
int webgfx_video_advance(int video, double seconds);
void webgfx_video_seek(int video, double seconds);
void webgfx_canvas_draw_video(int canvas, int video, float const* dst);
int webgfx_video_has_audio(int video);
// sound from seconds on; pause stops it
void webgfx_video_play(int video, double seconds);
void webgfx_video_pause(int video);
void webgfx_video_volume(int video, float gain);

// fragmented mp4 read as it arrives: tracks, then samples
int webgfx_demux_new(void);
void webgfx_demux_free(int demux);
int webgfx_demux_append(int demux, uint8_t const* bytes, int64_t size);
void webgfx_demux_reset(int demux);
int webgfx_demux_inits(int demux);
int webgfx_demux_tracks(int demux);
// id, kind 1 video 2 audio, codec, scale, w, h, ch, rate
void webgfx_demux_track(int demux, int index, int64_t* info);
int webgfx_demux_track_config(int demux, int index, uint8_t* out, int capacity);
int webgfx_demux_samples(int demux);
// track, dts, pts, duration, timescale, sync; the size
int64_t webgfx_demux_sample(int demux, int index, int64_t* info);
void webgfx_demux_sample_data(int demux, int index, uint8_t* out);
void webgfx_demux_clear(int demux);

// media source player; microseconds, its sound is the clock
int webgfx_stream_new(void);
void webgfx_stream_video_config(int video, uint8_t const* avcc, int size);
void webgfx_stream_video_sample(int video, uint8_t const* bytes, int64_t size, int64_t pts, int sync, int show);
void webgfx_stream_video_end(int video);
int webgfx_stream_audio_config(int video, uint8_t const* config, int size);
void webgfx_stream_audio_sample(int video, uint8_t const* bytes, int64_t size, int64_t pts);
void webgfx_stream_flush(int video, int kind);
int webgfx_stream_video_queued(int video);
int64_t webgfx_stream_audio_buffered(int video);
void webgfx_stream_playing(int video, int on);
void webgfx_stream_gain(int video, float gain);
int64_t webgfx_stream_time(int video);
int webgfx_stream_advance(int video, int64_t time);
void webgfx_stream_size(int video, int* width, int* height);
int webgfx_stream_next(int video, int64_t microseconds, int* width, int* height);
void webgfx_stream_planes(int video, uint8_t* y, uint8_t* u, uint8_t* v);
void webgfx_canvas_draw_image(int canvas, int image, float const* dst, float const* uv);
void webgfx_canvas_draw_canvas(int canvas, int source, float const* dst, float const* uv);
void webgfx_canvas_set_ramp(int canvas, int image, int kind, int spread, float alpha, float const* matrix, float const* points, float r0, float r1);
void webgfx_canvas_compose(int canvas, int layer, int mask, bool luminance, float alpha, int x, int y, int width, int height);
// css filters in pixels: blur (tint), color matrix, tables
void webgfx_canvas_filter_blur(int canvas, int source, float x, float y, float sigmaX, float sigmaY, float const* tint);
void webgfx_canvas_filter_matrix(int canvas, int source, float x, float y, float const* matrix);
void webgfx_canvas_filter_lut(int canvas, int source, float x, float y, uint8_t const* table);
void webgfx_canvas_set_composite(int canvas, int op);
void webgfx_canvas_set_shadow(int canvas, int slot, float const* rgba, float x, float y, float blur, float spread, bool inset);
void webgfx_canvas_clear_shadows(int canvas);
void webgfx_canvas_set_filter(int canvas, float hue, float saturate, float brightness);
void webgfx_canvas_clip_rounded_rect(int canvas, float x, float y, float width, float height, float const* radii);
void webgfx_canvas_clip_path(int canvas, int path, bool even_odd);
void webgfx_canvas_clip_out_path(int canvas, int path);
void webgfx_canvas_clip_out_rect(int canvas, float x, float y, float width, float height);
void webgfx_canvas_clip_image(int canvas, int image, float const* dst);
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

namespace WebCore::WebGfx {

// webgfx_path_point's first value
enum class PathCommand : uint8_t {
    Move = 0,
    Line = 1,
    Quad = 2,
    Cubic = 3,
};

}
