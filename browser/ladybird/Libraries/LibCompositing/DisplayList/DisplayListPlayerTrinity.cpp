/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteString.h>
#include <AK/HashTable.h>
#include <AK/Math.h>
#include <AK/NeverDestroyed.h>
#include <AK/Time.h>
#include <LibCompositing/DisplayList/DisplayListPlayerTrinity.h>
#include <LibCompositing/DisplayList/DisplayListResourceStorage.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/DecodedImageFrame.h>
#include <LibGfx/Font/Font.h>
#include <LibGfx/Font/TypefaceTrinity.h>
#include <LibGfx/PainterTrinity.h>
#include <LibGfx/PathTrinity.h>
#include <LibGfx/WebGfx.h>

namespace Compositing {

// one line per missing feature, so gaps show and never pass
static void not_ported(StringView what)
{
    static NeverDestroyed<HashTable<ByteString>> seen;
    if (seen->set(what) == HashSetResult::InsertedNewEntry)
        dbgln("trinity player: not ported: {}", what);
}

DisplayListPlayerTrinity::DisplayListPlayerTrinity() = default;
DisplayListPlayerTrinity::~DisplayListPlayerTrinity()
{
    for (auto const& layer : m_free_layers)
        webgfx_canvas_free(layer.id);
}

// a cleared canvas of at least size; pooled on a 128 px grid
DisplayListPlayerTrinity::Layer DisplayListPlayerTrinity::take_layer(Gfx::IntPoint origin, Gfx::IntSize size)
{
    size = { max(size.width(), 1), max(size.height(), 1) };
    Optional<size_t> best;
    for (size_t i = 0; i < m_free_layers.size(); ++i) {
        auto const& free = m_free_layers[i].capacity;
        if (free.width() < size.width() || free.height() < size.height())
            continue;
        if (!best.has_value() || free.area() < m_free_layers[*best].capacity.area())
            best = i;
    }
    Layer layer;
    if (best.has_value()) {
        layer = m_free_layers.take(*best);
        float const clear[4] = { 0, 0, 0, 0 };
        webgfx_canvas_clear(layer.id, clear);
    } else {
        auto round_up = [](int v) { return (v + 127) / 128 * 128; };
        layer.capacity = { round_up(size.width()), round_up(size.height()) };
        layer.id = webgfx_canvas_new(layer.capacity.width(), layer.capacity.height());
    }
    layer.origin = origin;
    layer.size = size;
    return layer;
}

Gfx::IntPoint DisplayListPlayerTrinity::layer_origin() const
{
    return m_layers.is_empty() ? Gfx::IntPoint {} : m_layers.last().origin;
}

// child contexts (iframes) are not ported, so no resolver is kept
void DisplayListPlayerTrinity::execute(DisplayList const& display_list, AccumulatedVisualContextTree const& tree,
    DisplayListResourceStorage const& resources, ScrollStateSnapshot const& scroll_state, RefPtr<Gfx::PaintingSurface> surface,
    CanvasSurfaceRegistry const* canvas_surface_registry, CompositedContextResolver const*)
{
    DisplayListPlayer::execute(display_list, tree, resources, scroll_state, move(surface), canvas_surface_registry);
}

void DisplayListPlayerTrinity::set_base_transform(Gfx::PaintingSurface& surface, Gfx::AffineTransform const& transform)
{
    m_matrix = transform;
    m_matrix_stack.clear();
    float const m[6] = { m_matrix.a(), m_matrix.b(), m_matrix.c(), m_matrix.d(), m_matrix.e(), m_matrix.f() };
    webgfx_canvas_transform(surface.webgfx_canvas(), m);
}

void DisplayListPlayerTrinity::flush(Gfx::PaintingSurface& surface)
{
    surface.flush();
}

// the readback waits for the GPU, so the frame is done here
void DisplayListPlayerTrinity::flush_async(Gfx::PaintingSurface& surface, Function<void()>&& callback)
{
    surface.flush();
    callback();
}

void DisplayListPlayerTrinity::save()
{
    webgfx_canvas_save(canvas());
    m_matrix_stack.append(m_matrix);
}

void DisplayListPlayerTrinity::pop()
{
    webgfx_canvas_restore(canvas());
    if (!m_matrix_stack.is_empty())
        m_matrix = m_matrix_stack.take_last();
}

// a layer draws with the page's matrix, moved to its origin
void DisplayListPlayerTrinity::apply_matrix()
{
    auto origin = layer_origin();
    float const m[6] = { m_matrix.a(), m_matrix.b(), m_matrix.c(), m_matrix.d(),
        m_matrix.e() - origin.x(), m_matrix.f() - origin.y() };
    webgfx_canvas_transform(canvas(), m);
}

void DisplayListPlayerTrinity::clip_device_rect(Gfx::FloatRect const& rect)
{
    auto device = m_matrix.map(rect).to_rounded<int>().translated(-layer_origin());
    webgfx_canvas_clip(canvas(), device.x(), device.y(), device.width(), device.height());
}

void DisplayListPlayerTrinity::fill_rect(Gfx::FloatRect const& rect, Gfx::Color color, Gfx::CornerRadii const& radii)
{
    float rgba[4];
    Gfx::color_to_rgba(color, rgba);
    if (!radii.has_any_radius()) {
        webgfx_canvas_fill_rect(canvas(), rect.x(), rect.y(), rect.width(), rect.height(), rgba, nullptr);
        return;
    }
    float const r[4] = {
        static_cast<float>(radii.top_left.horizontal_radius),
        static_cast<float>(radii.top_right.horizontal_radius),
        static_cast<float>(radii.bottom_right.horizontal_radius),
        static_cast<float>(radii.bottom_left.horizontal_radius),
    };
    webgfx_canvas_fill_rect(canvas(), rect.x(), rect.y(), rect.width(), rect.height(), rgba, r);
}

void DisplayListPlayerTrinity::stroke_path(Gfx::Path const& path, Gfx::Color color, float thickness)
{
    float rgba[4];
    Gfx::color_to_rgba(color, rgba);
    webgfx_canvas_stroke_path(canvas(), static_cast<Gfx::PathImplTrinity const&>(path.impl()).webgfx_path(), rgba, thickness);
}

void DisplayListPlayerTrinity::draw_frame(ImageFrameResourceId id, Gfx::FloatRect const& dst, Gfx::FloatRect const& src)
{
    auto const& bitmap = resource_storage().image_frame(id).bitmap();
    auto image = resource_storage().webgfx_image(id);
    float const d[4] = { dst.x(), dst.y(), dst.width(), dst.height() };
    float const uv[4] = {
        src.left() / bitmap.width(),
        src.top() / bitmap.height(),
        src.right() / bitmap.width(),
        src.bottom() / bitmap.height(),
    };
    webgfx_canvas_draw_image(canvas(), image, d, uv);
}

void DisplayListPlayerTrinity::draw_glyphs(FontResourceId font_id, DisplayListDataSpan glyph_span, Gfx::FloatPoint translation, float scale, Gfx::Color color)
{
    auto glyphs = inline_objects<DisplayListGlyph>(glyph_span);
    if (glyphs.is_empty() || !resource_storage().has_font(font_id))
        return;
    auto const& font = resource_storage().font(font_id);
    auto ascent = font.pixel_metrics().ascent;
    Vector<u32> ids;
    Vector<float> xs;
    Vector<float> ys;
    for (auto const& glyph : glyphs) {
        ids.append(glyph.glyph_id);
        xs.append(translation.x() + glyph.position.x() * scale);
        ys.append(translation.y() + (glyph.position.y() + ascent) * scale);
    }
    float rgba[4];
    Gfx::color_to_rgba(color, rgba);
    webgfx_canvas_glyphs(canvas(), as<Gfx::TypefaceTrinity>(font.typeface()).webgfx_font(), font.pixel_size() * scale,
        ids.data(), xs.data(), ys.data(), static_cast<int>(ids.size()), rgba);
}

void DisplayListPlayerTrinity::play_command(DrawGlyphRun const& command)
{
    if (command.orientation == Gfx::Orientation::Vertical)
        not_ported("vertical text"sv);
    draw_glyphs(command.font_id, command.glyphs, command.translation, command.scale, command.color);
}

void DisplayListPlayerTrinity::play_command(FillRect const& command)
{
    if (command.compositing_and_blending_operator != Gfx::CompositingAndBlendingOperator::Normal)
        not_ported("rect blend operators"sv);
    auto color = command.background_color_animation_effect == NO_EFFECT_NODE
        ? command.color
        : active_visual_context_tree().sampled_background_color(command.background_color_animation_effect).value_or(command.color);
    fill_rect(command.rect.to_type<float>(), color);
}

void DisplayListPlayerTrinity::play_command(PaintCaret const& command)
{
    if (!caret_is_visible_at_time(command, MonotonicTime::now().nanoseconds()))
        return;
    fill_rect(command.rect.to_type<float>(), command.color);
}

void DisplayListPlayerTrinity::play_command(DrawScaledDecodedImageFrame const& command)
{
    if (command.isolated_backdrop_color.has_value())
        not_ported("images over an isolated backdrop"sv);
    if (command.apply_force_dark)
        not_ported("force-dark images"sv);
    auto const& frame = resource_storage().image_frame(command.frame_id);
    auto src = command.src_rect.value_or(frame.rect().to_type<float>());
    save();
    clip_device_rect(command.dst_rect);
    draw_frame(command.frame_id, command.dst_rect, src);
    pop();
}

// tile origins every step from origin: count tiles on an axis,
// or (count 0) every tile that reaches into area
static void for_each_tile(Gfx::FloatPoint origin, Gfx::FloatSize step, Gfx::FloatRect const& area,
    u32 count_x, u32 count_y, Function<void(Gfx::FloatPoint)> const& draw)
{
    if (step.width() <= 0 || step.height() <= 0)
        return;
    auto first = [](float from, float by, float edge) { return from - AK::ceil((from - edge) / by) * by; };
    auto x0 = count_x ? origin.x() : first(origin.x(), step.width(), area.left());
    auto y0 = count_y ? origin.y() : first(origin.y(), step.height(), area.top());
    // a page-sized area of tiny tiles stops here
    constexpr u32 tile_limit = 16384;
    u32 drawn = 0;
    for (u32 j = 0; count_y ? j < count_y : y0 + j * step.height() < area.bottom(); ++j) {
        for (u32 i = 0; count_x ? i < count_x : x0 + i * step.width() < area.right(); ++i) {
            if (++drawn > tile_limit)
                return;
            draw({ x0 + i * step.width(), y0 + j * step.height() });
        }
    }
}

void DisplayListPlayerTrinity::play_command(DrawRepeatedDecodedImageFrame const& command)
{
    auto dst = command.dst_rect.to_type<float>();
    if (dst.is_empty())
        return;
    auto frame_rect = resource_storage().image_frame(command.frame_id).rect().to_type<float>();
    save();
    clip_device_rect(command.clip_rect.to_type<float>());
    for_each_tile(dst.location(), dst.size(), command.clip_rect.to_type<float>(), command.repeat.x ? 0 : 1, command.repeat.y ? 0 : 1,
        [&](Gfx::FloatPoint at) { draw_frame(command.frame_id, { at, dst.size() }, frame_rect); });
    pop();
}

// the tile's own records replay at each place, scaled to it
void DisplayListPlayerTrinity::play_command(DrawRepeatedTile const& command)
{
    auto tile_size = command.tile_size.to_type<float>();
    if (tile_size.is_empty() || command.dst_rect.is_empty())
        return;
    auto records = inline_data(command.tile);
    save();
    clip_device_rect(command.clip_rect.to_type<float>());
    for_each_tile(command.dst_rect.location(), command.tile_step, command.clip_rect.to_type<float>(), command.repeat.x ? 0 : 1, command.repeat.y ? 0 : 1,
        [&](Gfx::FloatPoint at) {
            save();
            m_matrix.translate(at);
            m_matrix.scale(command.dst_rect.width() / tile_size.width(), command.dst_rect.height() / tile_size.height());
            apply_matrix();
            execute_command_bytes(records, active_scroll_state());
            pop();
        });
    pop();
}

void DisplayListPlayerTrinity::play_command(DrawTiledDecodedImageFrame const& command)
{
    if (command.tile_rect.is_empty())
        return;
    save();
    clip_device_rect(command.clip_rect.to_type<float>());
    for_each_tile(command.tile_rect.location(), command.tile_step, command.clip_rect.to_type<float>(),
        command.tile_count_x.value_or(0), command.tile_count_y.value_or(0),
        [&](Gfx::FloatPoint at) { draw_frame(command.frame_id, { at, command.tile_rect.size() }, command.src_rect); });
    pop();
}

void DisplayListPlayerTrinity::play_command(DrawCompositedContext const&)
{
    not_ported("composited child contexts (iframes)"sv);
}

void DisplayListPlayerTrinity::play_command(DrawCanvas const&)
{
    not_ported("canvas elements"sv);
}

void DisplayListPlayerTrinity::play_command(DrawVideoFrame const& command)
{
    auto video = resource_storage().webgfx_video_for_sink(command.video_sink_id);
    if (video.size.is_empty())
        return;
    auto dst = command.dst_rect.to_type<float>();
    float const d[4] = { dst.x(), dst.y(), dst.width(), dst.height() };
    if (video.y != 0) {
        webgfx_canvas_draw_yuv(canvas(), video.y, video.u, video.v, d, video.rows);
        return;
    }
    float const uv[4] = { 0, 0, 1, 1 };
    webgfx_canvas_draw_image(canvas(), video.rgba, d, uv);
}

void DisplayListPlayerTrinity::play_command(PaintLinearGradient const&)
{
    not_ported("linear gradients"sv);
}

void DisplayListPlayerTrinity::play_command(PaintRadialGradient const&)
{
    not_ported("radial gradients"sv);
}

void DisplayListPlayerTrinity::play_command(PaintConicGradient const&)
{
    not_ported("conic gradients"sv);
}

void DisplayListPlayerTrinity::play_command(PaintOuterBoxShadow const&)
{
    not_ported("outer box shadows"sv);
}

void DisplayListPlayerTrinity::play_command(PaintInnerBoxShadow const&)
{
    not_ported("inner box shadows"sv);
}

void DisplayListPlayerTrinity::play_command(PaintTextShadow const&)
{
    not_ported("text shadows"sv);
}

void DisplayListPlayerTrinity::play_command(FillRectWithRoundedCorners const& command)
{
    auto color = command.background_color_animation_effect == NO_EFFECT_NODE
        ? command.color
        : active_visual_context_tree().sampled_background_color(command.background_color_animation_effect).value_or(command.color);
    fill_rect(command.rect.to_type<float>(), color, command.corner_radii);
}

void DisplayListPlayerTrinity::play_command(FillRoundedRectRing const& command)
{
    auto const& rect = command.rect;
    if (command.corner_radii.has_any_radius())
        not_ported("rounded border rings (drawn square)"sv);
    int top_width = clamp(command.top_width, 0, rect.height());
    int bottom_width = clamp(command.bottom_width, 0, rect.height() - top_width);
    int left_width = clamp(command.left_width, 0, rect.width());
    int right_width = clamp(command.right_width, 0, rect.width() - left_width);
    int inner_top = rect.y() + top_width;
    int inner_height = rect.height() - top_width - bottom_width;
    if (top_width > 0)
        fill_rect(Gfx::IntRect { rect.x(), rect.y(), rect.width(), top_width }.to_type<float>(), command.color);
    if (bottom_width > 0)
        fill_rect(Gfx::IntRect { rect.x(), inner_top + inner_height, rect.width(), bottom_width }.to_type<float>(), command.color);
    if (left_width > 0 && inner_height > 0)
        fill_rect(Gfx::IntRect { rect.x(), inner_top, left_width, inner_height }.to_type<float>(), command.color);
    if (right_width > 0 && inner_height > 0)
        fill_rect(Gfx::IntRect { rect.right() - right_width, inner_top, right_width, inner_height }.to_type<float>(), command.color);
}

void DisplayListPlayerTrinity::play_command(FillPath const& command)
{
    if (command.paint_kind != PathPaintKind::Color)
        not_ported("path gradients and patterns"sv);
    auto path = Gfx::Path::from_serialized_bytes(inline_data(command.path_data));
    float rgba[4];
    Gfx::color_to_rgba(command.color, rgba, command.opacity);
    webgfx_canvas_fill_path(canvas(), static_cast<Gfx::PathImplTrinity const&>(path.impl()).webgfx_path(), rgba, command.winding_rule == Gfx::WindingRule::EvenOdd);
}

void DisplayListPlayerTrinity::play_command(StrokePath const& command)
{
    if (command.paint_kind != PathPaintKind::Color)
        not_ported("stroke gradients and patterns"sv);
    if (command.dash_array.size != 0)
        not_ported("dashed strokes"sv);
    auto path = Gfx::Path::from_serialized_bytes(inline_data(command.path_data));
    float rgba[4];
    Gfx::color_to_rgba(command.color, rgba, command.opacity);
    webgfx_canvas_stroke_path(canvas(), static_cast<Gfx::PathImplTrinity const&>(path.impl()).webgfx_path(), rgba, command.thickness);
}

void DisplayListPlayerTrinity::play_command(DrawEllipse const& command)
{
    auto rect = command.rect.to_type<float>();
    auto rx = rect.width() / 2;
    auto ry = rect.height() / 2;
    Gfx::Path path;
    path.move_to({ rect.x(), rect.center().y() });
    path.elliptical_arc_to({ rect.right(), rect.center().y() }, { rx, ry }, 0, false, true);
    path.elliptical_arc_to({ rect.x(), rect.center().y() }, { rx, ry }, 0, false, true);
    path.close();
    stroke_path(path, command.color, command.thickness);
}

void DisplayListPlayerTrinity::play_command(DrawLine const& command)
{
    if (command.style != Gfx::LineStyle::Solid)
        not_ported("dotted and dashed lines (drawn solid)"sv);
    Gfx::Path path;
    path.move_to(command.from.to_type<float>());
    path.line_to(command.to.to_type<float>());
    stroke_path(path, command.color, command.thickness);
}

void DisplayListPlayerTrinity::play_command(BackdropFilterRegion const&)
{
    not_ported("backdrop filters"sv);
}

void DisplayListPlayerTrinity::play_command(DrawRect const& command)
{
    Gfx::Path path;
    auto rect = command.rect.to_type<float>();
    path.move_to(rect.top_left());
    path.line_to(rect.top_right());
    path.line_to(rect.bottom_right());
    path.line_to(rect.bottom_left());
    path.close();
    stroke_path(path, command.color, 1);
}

void DisplayListPlayerTrinity::play_command(PaintNestedDisplayList const& command)
{
    auto const& nested = resource_storage().display_list_resource(command.display_list_id);
    save();
    clip_device_rect(command.rect);
    m_matrix.translate(command.rect.x(), command.rect.y());
    if (!command.list_size.is_empty() && !command.rect.is_empty())
        m_matrix.scale(command.rect.width() / command.list_size.width(), command.rect.height() / command.list_size.height());
    apply_matrix();
    execute_nested_display_list(*nested.display_list, nested.visual_context_tree, active_scroll_state());
    pop();
}

void DisplayListPlayerTrinity::play_command(DrawIsolatedGroup const& command)
{
    if (command.opacity < 1.0f)
        not_ported("group opacity"sv);
    if (command.compositing_and_blending_operator != Gfx::CompositingAndBlendingOperator::Normal)
        not_ported("group blend operators"sv);
    if (command.filter.size != 0)
        not_ported("group filters"sv);
    if (command.mask.size != 0)
        not_ported("group masks"sv);
    save();
    if (command.clip_rect.has_value())
        clip_device_rect(*command.clip_rect);
    execute_command_bytes(inline_data(command.content), active_scroll_state());
    pop();
}

void DisplayListPlayerTrinity::play_command(DeclareMaskContent const& command)
{
    declare_mask_content(command.effect, inline_data(command.content));
}

void DisplayListPlayerTrinity::play_command(CompositorScrollNode const&) { }
void DisplayListPlayerTrinity::play_command(CompositorWheelHitTestTarget const&) { }
void DisplayListPlayerTrinity::play_command(CompositorWheelHitTestTargetWithCornerRadii const&) { }
void DisplayListPlayerTrinity::play_command(CompositorMainThreadWheelEventRegion const&) { }
void DisplayListPlayerTrinity::play_command(CompositorScrollbar const&) { }
void DisplayListPlayerTrinity::play_command(CompositorBlockingWheelEventRegion const&) { }
void DisplayListPlayerTrinity::play_command(CompositorSnapContainer const&) { }
void DisplayListPlayerTrinity::play_command(CompositorSnapArea const&) { }

static void paint_scrollbar_into(int canvas, PaintScrollBar const& command)
{
    float track[4];
    Gfx::color_to_rgba(command.track_color, track);
    auto gutter = command.gutter_rect;
    webgfx_canvas_fill_rect(canvas, gutter.x(), gutter.y(), gutter.width(), gutter.height(), track, nullptr);
    float thumb_color[4];
    Gfx::color_to_rgba(command.thumb_color, thumb_color);
    auto thumb = command.thumb_rect;
    auto radius = thumb.width() / 2.0f;
    float const radii[4] = { radius, radius, radius, radius };
    webgfx_canvas_fill_rect(canvas, thumb.x(), thumb.y(), thumb.width(), thumb.height(), thumb_color, radii);
}

void DisplayListPlayerTrinity::play_command(PaintScrollBar const& command)
{
    paint_scrollbar_into(canvas(), command);
}

// called between replays: the surface comes in, not from a replay
void DisplayListPlayerTrinity::paint_scrollbar(Gfx::PaintingSurface& surface, PaintScrollBar const& command)
{
    paint_scrollbar_into(surface.webgfx_canvas(), command);
}

void DisplayListPlayerTrinity::set_matrix(Gfx::FloatMatrix4x4 const& matrix)
{
    if (matrix[2, 0] != 0 || matrix[2, 1] != 0 || matrix[3, 0] != 0 || matrix[3, 1] != 0 || matrix[3, 3] != 1)
        not_ported("3D and perspective transforms (flattened to 2D)"sv);
    m_matrix = Gfx::AffineTransform { matrix[0, 0], matrix[1, 0], matrix[0, 1], matrix[1, 1], matrix[0, 3], matrix[1, 3] };
    apply_matrix();
}

Gfx::FloatMatrix4x4 DisplayListPlayerTrinity::canvas_matrix() const
{
    return m_matrix.to_matrix();
}

bool DisplayListPlayerTrinity::would_be_fully_clipped_by_painter(Gfx::IntRect) const
{
    return false;
}

void DisplayListPlayerTrinity::push_clip(ReplayClip const& clip)
{
    save();
    if (clip.mode == ClipMode::Difference) {
        not_ported("difference clips"sv);
        return;
    }
    if (clip.corner_radii.has_any_radius())
        not_ported("rounded clips (clipped square)"sv);
    clip_device_rect(clip.rect);
}

void DisplayListPlayerTrinity::push_clip_path(Gfx::Path const& path, Gfx::WindingRule)
{
    save();
    not_ported("clips to a path's shape (bounds used)"sv);
    clip_device_rect(path.bounding_box());
}

void DisplayListPlayerTrinity::push_transform(Gfx::AffineTransform const& transform)
{
    save();
    m_matrix.multiply(transform);
    apply_matrix();
}

void DisplayListPlayerTrinity::push_layer(ReplayLayer const& layer)
{
    save();
    if (layer.opacity < 1.0f)
        not_ported("layer opacity"sv);
    if (layer.blend_mode != Gfx::CompositingAndBlendingOperator::Normal)
        not_ported("layer blend modes"sv);
    if (layer.filter_bytes_size != 0 || layer.backdrop_filter_bytes_size != 0)
        not_ported("layer filters"sv);
}

// the masked content draws into a layer of its own
void DisplayListPlayerTrinity::push_mask(ReplayMask const& mask)
{
    save();
    clip_device_rect(mask.rect.to_type<float>());
    // the layer covers the mask's rect on the page, no more
    auto parent = layer_origin();
    auto parent_size = m_layers.is_empty() ? surface().size() : m_layers.last().size;
    auto area = m_matrix.map(mask.rect.to_type<float>()).to_rounded<int>();
    area.intersect(Gfx::IntRect { parent, parent_size });
    m_layers.append(take_layer(area.location(), area.size()));
    apply_matrix();
}

// the mask draws into a second layer; the content goes down
// through it. closes push_mask's save
void DisplayListPlayerTrinity::pop_mask(ReplayMask const& mask, EffectNodeIndex effect)
{
    auto content = m_layers.take_last();
    m_layers.append(take_layer(content.origin, content.size));
    apply_matrix();
    if (auto bytes = declared_mask_content(effect); bytes.has_value()) {
        save();
        clip_device_rect(mask.rect.to_type<float>());
        execute_command_bytes(*bytes, active_scroll_state());
        pop();
    }
    auto mask_layer = m_layers.take_last();
    auto at = content.origin - layer_origin();
    webgfx_canvas_draw_layer(canvas(), content.id, mask_layer.id, mask.kind == Gfx::MaskKind::Luminance,
        at.x(), at.y(), content.size.width(), content.size.height());
    m_free_layers.append(content);
    m_free_layers.append(mask_layer);
    pop();
}

void DisplayListPlayerTrinity::push_device_space_plane_clip(Gfx::Path const&)
{
    save();
    not_ported("device-space plane clips"sv);
}

}
