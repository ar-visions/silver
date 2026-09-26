/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteString.h>
#include <AK/HashTable.h>
#include <AK/NeverDestroyed.h>
#include <AK/Vector.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/DecodedImageFrame.h>
#include <LibGfx/Font/Font.h>
#include <LibGfx/Font/TypefaceTrinity.h>
#include <LibGfx/PainterTrinity.h>
#include <LibGfx/PathTrinity.h>
#include <LibGfx/TextLayout.h>
#include <LibGfx/WebGfx.h>

namespace Gfx {

// one line per missing feature, so gaps show and never pass
static void not_ported(StringView what)
{
    static NeverDestroyed<HashTable<ByteString>> seen;
    if (seen->set(what) == HashSetResult::InsertedNewEntry)
        dbgln("trinity painter: not ported: {}", what);
}

void color_to_rgba(Color color, float out[4], float alpha)
{
    out[0] = color.red() / 255.0f;
    out[1] = color.green() / 255.0f;
    out[2] = color.blue() / 255.0f;
    out[3] = color.alpha() / 255.0f * alpha;
}

static Optional<Color> solid_color(PaintStyle const& style)
{
    if (auto const* solid = as_if<SolidColorPaintStyle>(style))
        return solid->color();
    not_ported("gradient and pattern paint styles"sv);
    return {};
}

static int path_id(Path const& path)
{
    return static_cast<PathImplTrinity const&>(path.impl()).webgfx_path();
}

PainterTrinity::PainterTrinity(NonnullRefPtr<Gfx::PaintingSurface> painting_surface)
    : m_painting_surface(move(painting_surface))
{
    webgfx_canvas_save(canvas());
}

PainterTrinity::~PainterTrinity()
{
    while (m_save_depth > 0)
        restore();
    webgfx_canvas_restore(canvas());
    m_painting_surface->flush();
}

void PainterTrinity::clear_rect(Gfx::FloatRect const& rect, Gfx::Color color)
{
    if (color.alpha() != 255)
        not_ported("clear_rect to a see-through color (needs a replace blend)"sv);
    fill_rect(rect, color);
}

void PainterTrinity::fill_rect(Gfx::FloatRect const& rect, Color color)
{
    float rgba[4];
    color_to_rgba(color, rgba);
    webgfx_canvas_fill_rect(canvas(), rect.x(), rect.y(), rect.width(), rect.height(), rgba, nullptr);
}

void PainterTrinity::draw_bitmap(Gfx::FloatRect const& dst_rect, Gfx::DecodedImageFrame const& source, Gfx::IntRect const& src_rect, Gfx::ScalingMode, Optional<Gfx::Filter> filter, float global_alpha, Gfx::CompositingAndBlendingOperator compositing_and_blending_operator)
{
    if (filter.has_value())
        not_ported("image filters"sv);
    if (global_alpha < 1.0f)
        not_ported("image global alpha"sv);
    if (compositing_and_blending_operator != CompositingAndBlendingOperator::SourceOver)
        not_ported("image blend operators"sv);
    auto const& bitmap = source.bitmap();
    auto rgba = rgba_from_bitmap(bitmap);
    float const dst[4] = { dst_rect.x(), dst_rect.y(), dst_rect.width(), dst_rect.height() };
    float const uv[4] = {
        static_cast<float>(src_rect.left()) / bitmap.width(),
        static_cast<float>(src_rect.top()) / bitmap.height(),
        static_cast<float>(src_rect.right()) / bitmap.width(),
        static_cast<float>(src_rect.bottom()) / bitmap.height(),
    };
    webgfx_canvas_image(canvas(), rgba.data(), bitmap.width(), bitmap.height(), dst, uv);
}

void PainterTrinity::stroke_path(Gfx::Path const& path, Gfx::Color color, float thickness, float blur_radius, Gfx::CompositingAndBlendingOperator, Gfx::Path::CapStyle, Gfx::Path::JoinStyle, float, Vector<float> const& dash_array, float)
{
    if (blur_radius > 0)
        not_ported("blurred strokes"sv);
    if (!dash_array.is_empty())
        not_ported("dashed strokes"sv);
    float rgba[4];
    color_to_rgba(color, rgba);
    webgfx_canvas_stroke_path(canvas(), path_id(path), rgba, thickness);
}

void PainterTrinity::stroke_path(Gfx::Path const& path, Gfx::PaintStyle const& style, Optional<Gfx::Filter> filter, float thickness, float global_alpha, Gfx::CompositingAndBlendingOperator, Gfx::Path::CapStyle const&, Gfx::Path::JoinStyle const&, float, Vector<float> const& dash_array, float)
{
    if (filter.has_value())
        not_ported("path filters"sv);
    if (!dash_array.is_empty())
        not_ported("dashed strokes"sv);
    auto color = solid_color(style);
    if (!color.has_value())
        return;
    float rgba[4];
    color_to_rgba(*color, rgba, global_alpha);
    webgfx_canvas_stroke_path(canvas(), path_id(path), rgba, thickness);
}

void PainterTrinity::fill_path(Gfx::Path const& path, Gfx::Color color, Gfx::WindingRule winding_rule, float blur_radius, Gfx::CompositingAndBlendingOperator)
{
    if (blur_radius > 0)
        not_ported("blurred fills"sv);
    float rgba[4];
    color_to_rgba(color, rgba);
    webgfx_canvas_fill_path(canvas(), path_id(path), rgba, winding_rule == WindingRule::EvenOdd);
}

void PainterTrinity::fill_path(Gfx::Path const& path, Gfx::PaintStyle const& style, Optional<Gfx::Filter> filter, float global_alpha, Gfx::CompositingAndBlendingOperator, Gfx::WindingRule winding_rule)
{
    if (filter.has_value())
        not_ported("path filters"sv);
    auto color = solid_color(style);
    if (!color.has_value())
        return;
    float rgba[4];
    color_to_rgba(*color, rgba, global_alpha);
    webgfx_canvas_fill_path(canvas(), path_id(path), rgba, winding_rule == WindingRule::EvenOdd);
}

void PainterTrinity::draw_glyph_run(Gfx::GlyphRun const& glyph_run, FloatPoint position, PaintStyle const& style, Optional<Filter> filter, float global_alpha, CompositingAndBlendingOperator op)
{
    auto const& font = glyph_run.font();
    auto ascent = font.pixel_metrics().ascent;
    Vector<u32> ids;
    Vector<FloatPoint> baselines;
    for (auto const& glyph : glyph_run.glyphs()) {
        ids.append(glyph.glyph_id);
        baselines.append({ position.x() + glyph.position.x(), position.y() + glyph.position.y() + ascent });
    }
    draw_glyphs(font, 1.0f, ids, baselines, style, move(filter), global_alpha, op);
}

void PainterTrinity::draw_glyphs(Gfx::Font const& font, float scale, ReadonlySpan<u32> glyph_ids, ReadonlySpan<FloatPoint> baselines, PaintStyle const& style, Optional<Filter> filter, float global_alpha, CompositingAndBlendingOperator)
{
    if (filter.has_value())
        not_ported("text filters"sv);
    auto color = solid_color(style);
    if (!color.has_value() || glyph_ids.is_empty())
        return;
    Vector<float> xs;
    Vector<float> ys;
    for (auto const& baseline : baselines) {
        xs.append(baseline.x() * scale);
        ys.append(baseline.y() * scale);
    }
    float rgba[4];
    color_to_rgba(*color, rgba, global_alpha);
    webgfx_canvas_glyphs(canvas(), as<TypefaceTrinity>(font.typeface()).webgfx_font(), font.pixel_size() * scale,
        glyph_ids.data(), xs.data(), ys.data(), static_cast<int>(glyph_ids.size()), rgba);
}

void PainterTrinity::set_transform(Gfx::AffineTransform const& transform)
{
    float const m[6] = { transform.a(), transform.b(), transform.c(), transform.d(), transform.e(), transform.f() };
    webgfx_canvas_transform(canvas(), m);
}

void PainterTrinity::save()
{
    webgfx_canvas_save(canvas());
    ++m_save_depth;
}

void PainterTrinity::restore()
{
    if (m_save_depth == 0)
        return;
    webgfx_canvas_restore(canvas());
    --m_save_depth;
}

void PainterTrinity::clip(Gfx::Path const& path, Gfx::WindingRule)
{
    // trinity clips to device rects; a shape clip is its bounds
    not_ported("clips to a path's shape (bounds used)"sv);
    auto bounds = path.bounding_box().to_rounded<int>();
    webgfx_canvas_clip(canvas(), bounds.x(), bounds.y(), bounds.width(), bounds.height());
}

void PainterTrinity::reset()
{
    while (m_save_depth > 0)
        restore();
    webgfx_canvas_restore(canvas());
    webgfx_canvas_save(canvas());
}

}
