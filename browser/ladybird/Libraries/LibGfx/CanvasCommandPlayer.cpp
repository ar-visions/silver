/*
 * Copyright (c) 2026, Aliaksandr Kalenik <kalenik.aliaksandr@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Assertions.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/CanvasCommandPlayer.h>
#include <LibGfx/DecodedImageFrame.h>
#include <LibGfx/Font/Font.h>
#include <LibGfx/Color.h>
#include <LibGfx/PainterTrinity.h>
#include <LibGfx/PaintingSurface.h>

namespace Gfx {

CanvasCommandPlayer::CanvasCommandPlayer(IntSize size, BitmapFormat format, AlphaType alpha_type, CanvasSurfaceResolver canvas_surface_resolver, FontResolver font_resolver)
    : m_surface(PaintingSurface::create_with_size(size, format, alpha_type))
    , m_painter(make<PainterTrinity>(*m_surface))
    , m_canvas_surface_resolver(move(canvas_surface_resolver))
    , m_font_resolver(move(font_resolver))
{
}

CanvasCommandPlayer::~CanvasCommandPlayer() = default;

NonnullRefPtr<PaintingSurface> CanvasCommandPlayer::surface() const
{
    return m_surface;
}

void CanvasCommandPlayer::clear(Color color)
{
    m_painter->clear_rect({ {}, m_surface->size().to_type<float>() }, color);
}

void CanvasCommandPlayer::play(CanvasCommandList const& command_list)
{
    for (auto const& command : command_list.commands())
        command.visit([&](auto const& command) { play_command(command); });
}

void CanvasCommandPlayer::play_command(CanvasCommands::DrawGlyphRun const& command)
{
    if (!m_font_resolver)
        return;
    auto const* font = m_font_resolver(command.font_id);
    if (!font)
        return;
    auto ascent = font->pixel_metrics().ascent;
    Vector<u32> ids;
    Vector<FloatPoint> baselines;
    for (auto const& glyph : command.glyphs) {
        ids.append(glyph.glyph_id);
        baselines.append({ command.translation.x() + glyph.position.x(), command.translation.y() + glyph.position.y() + ascent });
    }
    m_painter->draw_glyphs(*font, 1.0f, ids, baselines, resolve_paint_style(command.style), command.filter, command.global_alpha, command.compositing_and_blending_operator);
}

void CanvasCommandPlayer::play_command(CanvasCommands::ClearRect const& command)
{
    m_painter->clear_rect(command.rect, command.color);
}

void CanvasCommandPlayer::play_command(CanvasCommands::FillRect const& command)
{
    m_painter->fill_rect(command.rect, command.color);
}

void CanvasCommandPlayer::play_command(CanvasCommands::DrawBitmap const& command)
{
    m_painter->draw_bitmap(command.dst_rect, command.frame, command.src_rect, command.scaling_mode, command.filter, command.global_alpha, command.compositing_and_blending_operator);
}

void CanvasCommandPlayer::play_command(CanvasCommands::DrawCanvas const& command)
{
    if (!m_canvas_surface_resolver)
        return;

    auto* source_surface = m_canvas_surface_resolver(command.source_canvas_id);
    if (!source_surface)
        return;

    auto bitmap = source_surface->snapshot_bitmap();
    m_painter->draw_bitmap(command.dst_rect, DecodedImageFrame { *bitmap }, command.src_rect, command.scaling_mode, command.filter, command.global_alpha, command.compositing_and_blending_operator);
}

void CanvasCommandPlayer::play_command(CanvasCommands::FillPath const& command)
{
    // Shadows are recorded as blurred solid-color fills; everything else goes through the general paint-style overload.
    if (command.blur_radius > 0 && command.style.has<Color>()) {
        m_painter->fill_path(command.path, command.style.get<Color>(), command.winding_rule, command.blur_radius, command.compositing_and_blending_operator);
        return;
    }

    m_painter->fill_path(command.path, resolve_paint_style(command.style), command.filter, command.global_alpha, command.compositing_and_blending_operator, command.winding_rule);
}

void CanvasCommandPlayer::play_command(CanvasCommands::StrokePath const& command)
{
    if (command.blur_radius > 0 && command.style.has<Color>()) {
        m_painter->stroke_path(command.path, command.style.get<Color>(), command.thickness, command.blur_radius, command.compositing_and_blending_operator, command.cap_style, command.join_style, command.miter_limit, command.dash_array, command.dash_offset);
        return;
    }

    m_painter->stroke_path(command.path, resolve_paint_style(command.style), command.filter, command.thickness, command.global_alpha, command.compositing_and_blending_operator, command.cap_style, command.join_style, command.miter_limit, command.dash_array, command.dash_offset);
}

void CanvasCommandPlayer::play_command(CanvasCommands::SetTransform const& command)
{
    m_painter->set_transform(command.transform);
}

void CanvasCommandPlayer::play_command(CanvasCommands::Save const&)
{
    m_painter->save();
}

void CanvasCommandPlayer::play_command(CanvasCommands::Restore const&)
{
    m_painter->restore();
}

void CanvasCommandPlayer::play_command(CanvasCommands::ClipPath const& command)
{
    m_painter->clip(command.path, command.winding_rule);
}

void CanvasCommandPlayer::play_command(CanvasCommands::Reset const&)
{
    m_painter->reset();
}

NonnullRefPtr<PaintStyle> CanvasCommandPlayer::resolve_paint_style(CanvasPaintStyle const& style) const
{
    auto with_color_stops = [](auto paint_style, auto const& gradient) -> NonnullRefPtr<PaintStyle> {
        paint_style->set_color_stops(Vector<ColorStop> { gradient.color_stops });
        if (gradient.repeat_length.has_value())
            paint_style->set_repeat_length(*gradient.repeat_length);
        return paint_style;
    };

    return style.visit(
        [](Color const& color) -> NonnullRefPtr<PaintStyle> {
            return MUST(SolidColorPaintStyle::create(color));
        },
        [&](CanvasLinearGradient const& gradient) -> NonnullRefPtr<PaintStyle> {
            return with_color_stops(MUST(CanvasLinearGradientPaintStyle::create(gradient.start_point, gradient.end_point)), gradient);
        },
        [&](CanvasRadialGradient const& gradient) -> NonnullRefPtr<PaintStyle> {
            return with_color_stops(MUST(CanvasRadialGradientPaintStyle::create(gradient.start_center, gradient.start_radius, gradient.end_center, gradient.end_radius)), gradient);
        },
        [&](CanvasConicGradient const& gradient) -> NonnullRefPtr<PaintStyle> {
            return with_color_stops(MUST(CanvasConicGradientPaintStyle::create(gradient.center, gradient.start_angle)), gradient);
        },
        [&](CanvasPatternStyle const& pattern) -> NonnullRefPtr<PaintStyle> {
            auto paint_style = MUST(CanvasPatternPaintStyle::create(pattern.image, pattern.repetition));
            if (pattern.transform.has_value())
                paint_style->set_transform(*pattern.transform);
            return paint_style;
        });
}

}
