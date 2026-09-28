/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Vector.h>
#include <LibCompositing/DisplayList/CompositedContext.h>
#include <LibCompositing/DisplayList/DisplayList.h>
#include <LibGfx/AffineTransform.h>
#include <LibGfx/PaintingSurface.h>

namespace Compositing {

// replays a display list onto a trinity Canvas (webgfx)
class COMPOSITING_API DisplayListPlayerTrinity final : public DisplayListPlayer {
public:
    AK_ALLOC_WITH_KMALLOC;

    DisplayListPlayerTrinity();
    ~DisplayListPlayerTrinity();

    using DisplayListPlayer::execute;
    void execute(DisplayList const&, AccumulatedVisualContextTree const&, DisplayListResourceStorage const&,
        ScrollStateSnapshot const&, RefPtr<Gfx::PaintingSurface>, CanvasSurfaceRegistry const*, CompositedContextResolver const*);

    void flush(Gfx::PaintingSurface&) override;
    void flush_async(Gfx::PaintingSurface&, Function<void()>&&);
    void paint_scrollbar(Gfx::PaintingSurface&, PaintScrollBar const&);

    // the host's placement (raster scale, offset) under the list
    void set_base_transform(Gfx::PaintingSurface&, Gfx::AffineTransform const&);

private:
#define DECLARE_PLAY_COMMAND(command_type, player_method) \
    void play_command(command_type const&) override;
    ENUMERATE_DISPLAY_LIST_COMMANDS(DECLARE_PLAY_COMMAND)
#undef DECLARE_PLAY_COMMAND
    void set_matrix(Gfx::FloatMatrix4x4 const&) override;
    Gfx::FloatMatrix4x4 canvas_matrix() const override;
    bool would_be_fully_clipped_by_painter(Gfx::IntRect) const override;

    void push_clip(ReplayClip const&) override;
    void push_clip_path(Gfx::Path const&, Gfx::WindingRule) override;
    void push_transform(Gfx::AffineTransform const&) override;
    void push_layer(ReplayLayer const&) override;
    void push_mask(ReplayMask const&) override;
    void pop_mask(ReplayMask const&, EffectNodeIndex) override;
    void pop() override;
    void push_device_space_plane_clip(Gfx::Path const&) override;

    // an offscreen canvas at origin on the page
    struct Layer {
        int id { 0 };
        Gfx::IntPoint origin;
        Gfx::IntSize size;
        Gfx::IntSize capacity;
    };

    // the innermost open layer, else the surface
    int canvas() const { return m_layers.is_empty() ? surface().webgfx_canvas() : m_layers.last().id; }
    Layer take_layer(Gfx::IntPoint origin, Gfx::IntSize);
    Gfx::IntPoint layer_origin() const;
    void save();
    void apply_matrix();
    void clip_device_rect(Gfx::FloatRect const&);
    void fill_rect(Gfx::FloatRect const&, Gfx::Color, Gfx::CornerRadii const& = {});
    void stroke_path(Gfx::Path const&, Gfx::Color, float thickness);
    void draw_frame(ImageFrameResourceId, Gfx::FloatRect const& dst, Gfx::FloatRect const& src);
    void draw_glyphs(FontResourceId, DisplayListDataSpan glyphs, Gfx::FloatPoint translation, float scale, Gfx::Color);

    Gfx::AffineTransform m_matrix;
    Vector<Gfx::AffineTransform> m_matrix_stack;
    Vector<Layer> m_layers;
    // canvases kept for reuse
    Vector<Layer> m_free_layers;
};

}
