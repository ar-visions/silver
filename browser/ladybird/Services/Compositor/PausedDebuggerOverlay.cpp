/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Math.h>
#include <AK/Utf16String.h>
#include <Compositor/PausedDebuggerOverlay.h>
#include <LibCompositing/PausedDebuggerOverlay.h>
#include <LibGfx/Font/Font.h>
#include <LibGfx/Font/FontDatabase.h>
#include <LibGfx/PaintStyle.h>
#include <LibGfx/PainterTrinity.h>
#include <LibGfx/PaintingSurface.h>
#include <LibGfx/Path.h>
#include <LibGfx/TextLayout.h>
#include <LibGfx/WebGfx.h>

namespace Compositor {

static void fill_rounded(int canvas, Gfx::IntRect const& rect, float radius, Color color)
{
    float rgba[4];
    Gfx::color_to_rgba(color, rgba);
    float const radii[4] = { radius, radius, radius, radius };
    webgfx_canvas_fill_rect(canvas, rect.x(), rect.y(), rect.width(), rect.height(), rgba, radii);
}

static Gfx::Path rounded_rect_path(Gfx::FloatRect const& r, float radius)
{
    Gfx::Path path;
    path.move_to({ r.left() + radius, r.top() });
    path.line_to({ r.right() - radius, r.top() });
    path.elliptical_arc_to({ r.right(), r.top() + radius }, { radius, radius }, 0, false, true);
    path.line_to({ r.right(), r.bottom() - radius });
    path.elliptical_arc_to({ r.right() - radius, r.bottom() }, { radius, radius }, 0, false, true);
    path.line_to({ r.left() + radius, r.bottom() });
    path.elliptical_arc_to({ r.left(), r.bottom() - radius }, { radius, radius }, 0, false, true);
    path.line_to({ r.left(), r.top() + radius });
    path.elliptical_arc_to({ r.left() + radius, r.top() }, { radius, radius }, 0, false, true);
    path.close();
    return path;
}

void paint_paused_debugger_overlay(Gfx::PaintingSurface& surface, Gfx::IntSize viewport_size, double device_pixel_ratio, Optional<String> const& font_family, Optional<Compositing::PausedDebuggerOverlayAction> hovered_action)
{
    // Light mode colors, as the other Ladybird frontends resolve them.
    static constexpr auto overlay_color = Color(0xee, 0xee, 0xef, 168);
    static constexpr auto toolbar_color = Color(0xff, 0xff, 0xff);
    static constexpr auto toolbar_border_color = Color(0xce, 0xce, 0xcf);
    static constexpr auto button_hover_color = Color(0xf1, 0xf1, 0xf2);
    static constexpr auto text_color = Color(0x18, 0x1d, 0x24);
    static constexpr auto shadow_color = Color(0x18, 0x1d, 0x24, 66);

    Gfx::PainterTrinity painter(surface);
    auto canvas = surface.webgfx_canvas();
    auto geometry = Compositing::paused_debugger_overlay_geometry(viewport_size, device_pixel_ratio);
    auto radius = max(1.0f, static_cast<float>(4 * device_pixel_ratio));

    painter.fill_rect({ 0, 0, static_cast<float>(viewport_size.width()), static_cast<float>(viewport_size.height()) }, overlay_color);

    auto shadow_rect = geometry.toolbar.translated(0, max(1, static_cast<int>(round(2 * device_pixel_ratio))));
    fill_rounded(canvas, shadow_rect, radius, shadow_color);
    fill_rounded(canvas, geometry.toolbar, radius, toolbar_color);

    if (hovered_action.has_value()) {
        auto const& hovered_button = *hovered_action == Compositing::PausedDebuggerOverlayAction::StepOver
            ? geometry.step_over_button
            : geometry.continue_button;
        painter.save();
        webgfx_canvas_clip(canvas, geometry.toolbar.x(), geometry.toolbar.y(), geometry.toolbar.width(), geometry.toolbar.height());
        painter.fill_rect(hovered_button.to_type<float>(), button_hover_color);
        painter.restore();
    }

    painter.stroke_path(rounded_rect_path(geometry.toolbar.to_type<float>(), radius), toolbar_border_color,
        static_cast<float>(max(1.0, device_pixel_ratio)), 0, Gfx::CompositingAndBlendingOperator::SourceOver,
        Gfx::Path::CapStyle::Butt, Gfx::Path::JoinStyle::Miter, 4, {}, 0);

    RefPtr<Gfx::Font> font;
    if (font_family.has_value())
        font = Gfx::FontDatabase::the().get(FlyString { *font_family }, 12, 400, Gfx::FontWidth::Normal, 0);
    if (!font)
        font = Gfx::FontDatabase::the().get("SerenitySans"_fly_string, 12, 400, Gfx::FontWidth::Normal, 0);
    auto text_style = MUST(Gfx::SolidColorPaintStyle::create(text_color));
    if (font) {
        auto scaled_font = font->with_size(font->point_size() * static_cast<float>(device_pixel_ratio));
        auto label = Utf16String::from_utf8("Paused in debugger"sv);
        auto label_width = Gfx::measure_text_width(label, *scaled_font);
        auto const& metrics = scaled_font->pixel_metrics();
        auto label_x = geometry.message.x() + (geometry.message.width() - label_width) / 2;
        auto label_y = geometry.message.center().y() + (metrics.ascent - metrics.descent) / 2;
        auto run = Gfx::shape_text({ label_x, label_y }, 0, 0, label, *scaled_font, Gfx::GlyphRun::TextType::Ltr);
        painter.draw_glyph_run(*run, {}, *text_style, {}, 1, Gfx::CompositingAndBlendingOperator::SourceOver);
    }

    auto icon_size = static_cast<float>(max(8.0, 16 * device_pixel_ratio));
    auto line_thickness = static_cast<float>(max(1.0, device_pixel_ratio));

    auto paint_continue_icon = [&](Gfx::IntRect const& button_rect) {
        auto icon_left = button_rect.center().x() - icon_size / 2;
        auto icon_top = button_rect.center().y() - icon_size / 2;
        Gfx::Path path;
        path.move_to({ icon_left + icon_size / 4, icon_top + icon_size / 8 });
        path.line_to({ icon_left + icon_size / 4, icon_top + icon_size - icon_size / 8 });
        path.line_to({ icon_left + icon_size - icon_size / 8, icon_top + icon_size / 2 });
        path.close();
        painter.fill_path(path, text_color, Gfx::WindingRule::Nonzero, 0, Gfx::CompositingAndBlendingOperator::SourceOver);
    };

    auto paint_step_over_icon = [&](Gfx::IntRect const& button_rect) {
        auto center = button_rect.center();
        auto left = center.x() - icon_size / 2;
        auto top = center.y() - icon_size / 2;
        auto right = center.x() + icon_size / 2;

        Gfx::Path arc;
        arc.move_to({ left + icon_size / 8, top + icon_size * 0.55f });
        arc.cubic_bezier_curve_to(
            { left + icon_size / 8, top + icon_size * 0.15f },
            { right - icon_size / 5, top + icon_size * 0.15f },
            { right - icon_size / 5, top + icon_size * 0.55f });
        painter.stroke_path(arc, text_color, line_thickness, 0, Gfx::CompositingAndBlendingOperator::SourceOver,
            Gfx::Path::CapStyle::Round, Gfx::Path::JoinStyle::Round, 4, {}, 0);

        Gfx::Path arrow;
        auto arrow_width = icon_size / 2.5f;
        arrow.move_to({ right - arrow_width, top + icon_size * 0.55f });
        arrow.line_to({ right, top + icon_size * 0.55f });
        arrow.line_to({ right - arrow_width / 2, top + icon_size * 0.8f });
        arrow.close();
        painter.fill_path(arrow, text_color, Gfx::WindingRule::Nonzero, 0, Gfx::CompositingAndBlendingOperator::SourceOver);

        auto dot_radius = max(2.0f, line_thickness * 2) / 2;
        auto dot_center = Gfx::FloatPoint { static_cast<float>(center.x()), center.y() + icon_size / 3 };
        Gfx::Path dot;
        dot.move_to({ dot_center.x() - dot_radius, dot_center.y() });
        dot.elliptical_arc_to({ dot_center.x() + dot_radius, dot_center.y() }, { dot_radius, dot_radius }, 0, false, true);
        dot.elliptical_arc_to({ dot_center.x() - dot_radius, dot_center.y() }, { dot_radius, dot_radius }, 0, false, true);
        dot.close();
        painter.fill_path(dot, text_color, Gfx::WindingRule::Nonzero, 0, Gfx::CompositingAndBlendingOperator::SourceOver);
    };

    paint_step_over_icon(geometry.step_over_button);
    paint_continue_icon(geometry.continue_button);
}

}
