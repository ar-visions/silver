/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Math.h>
#include <AK/StringBuilder.h>
#include <LibGfx/Font/Font.h>
#include <LibGfx/Font/TypefaceTrinity.h>
#include <LibGfx/PathTrinity.h>
#include <LibGfx/TextLayout.h>
#include <LibGfx/WebGfx.h>

namespace Gfx {

static constexpr size_t floats_per_point = 7;

NonnullOwnPtr<Gfx::PathImplTrinity> PathImplTrinity::create()
{
    return adopt_own(*new PathImplTrinity);
}

PathImplTrinity::PathImplTrinity()
    : m_path(webgfx_path_new())
{
}

PathImplTrinity::PathImplTrinity(PathImplTrinity const& other)
    : m_path(webgfx_path_clone(other.m_path))
    , m_last_move_to(other.m_last_move_to)
    , m_has_current_point(other.m_has_current_point)
    , m_fill_type(other.m_fill_type)
{
}

PathImplTrinity::~PathImplTrinity()
{
    webgfx_path_free(m_path);
}

void PathImplTrinity::clear()
{
    webgfx_path_free(m_path);
    m_path = webgfx_path_new();
    m_last_move_to = {};
    m_has_current_point = false;
}

void PathImplTrinity::move_to(Gfx::FloatPoint const& point)
{
    m_last_move_to = point;
    m_has_current_point = true;
    webgfx_path_move_to(m_path, point.x(), point.y());
}

void PathImplTrinity::line_to(Gfx::FloatPoint const& point)
{
    if (!m_has_current_point) {
        move_to(point);
        return;
    }
    webgfx_path_line_to(m_path, point.x(), point.y());
}

void PathImplTrinity::close()
{
    if (!m_has_current_point)
        return;
    webgfx_path_close(m_path);
    webgfx_path_move_to(m_path, m_last_move_to.x(), m_last_move_to.y());
}

void PathImplTrinity::elliptical_arc_to(FloatPoint point, FloatSize radii, float x_axis_rotation, bool large_arc, bool sweep)
{
    if (!m_has_current_point) {
        move_to(point);
        return;
    }
    webgfx_path_arc_to(m_path, point.x(), point.y(), radii.width(), radii.height(), x_axis_rotation, large_arc, sweep);
}

void PathImplTrinity::arc_to(FloatPoint point, float radius, bool large_arc, bool sweep)
{
    if (!m_has_current_point) {
        move_to(point);
        return;
    }
    webgfx_path_arc_to(m_path, point.x(), point.y(), radius, radius, 0, large_arc, sweep);
}

void PathImplTrinity::quadratic_bezier_curve_to(FloatPoint through, FloatPoint point)
{
    if (!m_has_current_point)
        move_to(through);
    webgfx_path_quad_to(m_path, through.x(), through.y(), point.x(), point.y());
}

void PathImplTrinity::cubic_bezier_curve_to(FloatPoint c1, FloatPoint c2, FloatPoint p2)
{
    if (!m_has_current_point)
        move_to(c1);
    webgfx_path_cubic_to(m_path, c1.x(), c1.y(), c2.x(), c2.y(), p2.x(), p2.y());
}

// replays another path's points onto this one
void PathImplTrinity::append_points_of(int path)
{
    auto count = webgfx_path_count(path);
    float p[floats_per_point];
    for (int i = 0; i < count; ++i) {
        webgfx_path_point(path, i, p);
        switch (static_cast<WebGfx::PathCommand>(p[0])) {
        case WebGfx::PathCommand::Move:
            webgfx_path_move_to(m_path, p[1], p[2]);
            m_last_move_to = { p[1], p[2] };
            m_has_current_point = true;
            break;
        case WebGfx::PathCommand::Line:
            webgfx_path_line_to(m_path, p[1], p[2]);
            break;
        case WebGfx::PathCommand::Quad:
            webgfx_path_quad_to(m_path, p[3], p[4], p[1], p[2]);
            break;
        case WebGfx::PathCommand::Cubic:
            webgfx_path_cubic_to(m_path, p[3], p[4], p[5], p[6], p[1], p[2]);
            break;
        }
    }
}

void PathImplTrinity::glyph_run(GlyphRun const& glyph_run)
{
    auto const& font = glyph_run.font();
    if (font.is_invisible())
        return;
    auto webgfx_font = as<TypefaceTrinity>(font.typeface()).webgfx_font();
    auto ascent = font.pixel_metrics().ascent;
    m_fill_type = WindingRule::Nonzero;
    for (auto const& glyph : glyph_run.glyphs())
        webgfx_font_outline(webgfx_font, glyph.glyph_id, m_path, glyph.position.x(), glyph.position.y() + ascent, font.pixel_size());
    m_has_current_point = webgfx_path_count(m_path) > 0;
}

void PathImplTrinity::offset(Gfx::FloatPoint const& offset)
{
    float const m[6] = { 1, 0, 0, 1, offset.x(), offset.y() };
    webgfx_path_transform(m_path, m);
    if (m_has_current_point)
        m_last_move_to.translate_by(offset);
}

NonnullOwnPtr<PathImpl> PathImplTrinity::place_glyph_runs_along(ReadonlySpan<NonnullRefPtr<GlyphRun>> glyph_runs, float offset) const
{
    auto path_length = webgfx_path_length(m_path);
    auto output = PathImplTrinity::create();
    for (auto const& glyph_run : glyph_runs) {
        auto const& font = glyph_run->font();
        if (font.is_invisible())
            continue;
        auto webgfx_font = as<TypefaceTrinity>(font.typeface()).webgfx_font();
        for (auto const& glyph : glyph_run->glyphs()) {
            auto distance = offset + glyph.position.x();
            float at[4] = {};
            webgfx_path_point_at(m_path, distance, at);
            if (at[3] == 0)
                continue;
            if (distance + glyph.glyph_width / 2.0f > path_length)
                return output;
            auto glyph_path = webgfx_path_new();
            webgfx_font_outline(webgfx_font, glyph.glyph_id, glyph_path, 0, 0, font.pixel_size());
            auto c = AK::cos(at[2]);
            auto s = AK::sin(at[2]);
            float const m[6] = { c, s, -s, c, at[0], at[1] };
            webgfx_path_transform(glyph_path, m);
            output->append_points_of(glyph_path);
            webgfx_path_free(glyph_path);
        }
    }
    return output;
}

void PathImplTrinity::append_path(Gfx::Path const& other)
{
    auto const& other_impl = static_cast<PathImplTrinity const&>(other.impl());
    append_points_of(other_impl.m_path);
    if (other_impl.m_has_current_point) {
        m_has_current_point = true;
        m_last_move_to = other_impl.m_last_move_to;
    }
}

void PathImplTrinity::intersect(Gfx::Path const&)
{
    // no caller; a boolean path op is not ported
    VERIFY_NOT_REACHED();
}

// fill rule byte, then 7 floats per point
Vector<u8> PathImplTrinity::serialize_to_bytes() const
{
    auto count = webgfx_path_count(m_path);
    Vector<u8> bytes;
    bytes.resize(1 + count * floats_per_point * sizeof(float));
    bytes[0] = static_cast<u8>(m_fill_type);
    auto* out = reinterpret_cast<float*>(bytes.data() + 1);
    for (int i = 0; i < count; ++i) {
        float p[floats_per_point];
        webgfx_path_point(m_path, i, p);
        __builtin_memcpy(out + i * floats_per_point, p, sizeof(p));
    }
    return bytes;
}

void PathImplTrinity::deserialize_from_bytes(ReadonlyBytes bytes)
{
    clear();
    if (bytes.is_empty())
        return;
    m_fill_type = static_cast<WindingRule>(bytes[0]);
    auto count = (bytes.size() - 1) / (floats_per_point * sizeof(float));
    auto source = webgfx_path_new();
    for (size_t i = 0; i < count; ++i) {
        float p[floats_per_point];
        __builtin_memcpy(p, bytes.data() + 1 + i * sizeof(p), sizeof(p));
        switch (static_cast<WebGfx::PathCommand>(p[0])) {
        case WebGfx::PathCommand::Move:
            webgfx_path_move_to(source, p[1], p[2]);
            break;
        case WebGfx::PathCommand::Line:
            webgfx_path_line_to(source, p[1], p[2]);
            break;
        case WebGfx::PathCommand::Quad:
            webgfx_path_quad_to(source, p[3], p[4], p[1], p[2]);
            break;
        case WebGfx::PathCommand::Cubic:
            webgfx_path_cubic_to(source, p[3], p[4], p[5], p[6], p[1], p[2]);
            break;
        }
    }
    append_points_of(source);
    webgfx_path_free(source);
}

bool PathImplTrinity::is_empty() const
{
    return !m_has_current_point;
}

Gfx::FloatPoint PathImplTrinity::last_point() const
{
    if (webgfx_path_count(m_path) == 0)
        return {};
    float cursor[2];
    webgfx_path_cursor(m_path, cursor);
    return { cursor[0], cursor[1] };
}

Gfx::FloatRect PathImplTrinity::bounding_box() const
{
    float b[4] = {};
    webgfx_path_bounds(m_path, b);
    return { b[0], b[1], b[2], b[3] };
}

float PathImplTrinity::length() const
{
    return webgfx_path_length(m_path);
}

bool PathImplTrinity::contains(FloatPoint point, Gfx::WindingRule winding_rule) const
{
    return webgfx_path_contains(m_path, point.x(), point.y(), winding_rule == WindingRule::EvenOdd);
}

void PathImplTrinity::set_fill_type(Gfx::WindingRule winding_rule)
{
    m_fill_type = winding_rule;
}

NonnullOwnPtr<PathImpl> PathImplTrinity::clone() const
{
    return adopt_own(*new PathImplTrinity(*this));
}

NonnullOwnPtr<PathImpl> PathImplTrinity::copy_transformed(Gfx::AffineTransform const& transform) const
{
    auto new_path = adopt_own(*new PathImplTrinity(*this));
    float const m[6] = { transform.a(), transform.b(), transform.c(), transform.d(), transform.e(), transform.f() };
    webgfx_path_transform(new_path->m_path, m);
    if (new_path->m_has_current_point)
        new_path->m_last_move_to = transform.map(new_path->m_last_move_to);
    return new_path;
}

String PathImplTrinity::to_svg_string() const
{
    StringBuilder builder;
    auto count = webgfx_path_count(m_path);
    for (int i = 0; i < count; ++i) {
        float p[floats_per_point];
        webgfx_path_point(m_path, i, p);
        if (i > 0)
            builder.append(' ');
        switch (static_cast<WebGfx::PathCommand>(p[0])) {
        case WebGfx::PathCommand::Move:
            builder.appendff("M {} {}", p[1], p[2]);
            break;
        case WebGfx::PathCommand::Line:
            builder.appendff("L {} {}", p[1], p[2]);
            break;
        case WebGfx::PathCommand::Quad:
            builder.appendff("Q {} {} {} {}", p[3], p[4], p[1], p[2]);
            break;
        case WebGfx::PathCommand::Cubic:
            builder.appendff("C {} {} {} {} {} {}", p[3], p[4], p[5], p[6], p[1], p[2]);
            break;
        }
    }
    return builder.to_string_without_validation();
}

}
