/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/NonnullRefPtr.h>
#include <LibGfx/CompositingAndBlendingOperator.h>
#include <LibGfx/PaintStyle.h>
#include <LibGfx/Painter.h>
#include <LibGfx/PaintingSurface.h>
#include <LibGfx/Path.h>
#include <LibGfx/WindingRule.h>

namespace Gfx {

// Painter over a trinity Canvas (webgfx)
class PainterTrinity final : public Painter {
public:
    explicit PainterTrinity(NonnullRefPtr<Gfx::PaintingSurface>);
    virtual ~PainterTrinity() override;

    virtual void clear_rect(Gfx::FloatRect const&, Color) override;
    virtual void fill_rect(Gfx::FloatRect const&, Color) override;
    virtual void draw_bitmap(Gfx::FloatRect const& dst_rect, Gfx::DecodedImageFrame const& source, Gfx::IntRect const& src_rect, Gfx::ScalingMode, Optional<Gfx::Filter>, float global_alpha, Gfx::CompositingAndBlendingOperator compositing_and_blending_operator) override;

    void stroke_path(Gfx::Path const&, Gfx::Color, float thickness, float blur_radius, Gfx::CompositingAndBlendingOperator compositing_and_blending_operator, Gfx::Path::CapStyle, Gfx::Path::JoinStyle, float miter_limit, Vector<float> const& dash_array, float dash_offset);
    void stroke_path(Gfx::Path const&, Gfx::PaintStyle const&, Optional<Gfx::Filter>, float thickness, float global_alpha, Gfx::CompositingAndBlendingOperator compositing_and_blending_operator, Gfx::Path::CapStyle const&, Gfx::Path::JoinStyle const&, float miter_limit, Vector<float> const&, float dash_offset);
    void fill_path(Gfx::Path const&, Gfx::Color, Gfx::WindingRule, float blur_radius, Gfx::CompositingAndBlendingOperator compositing_and_blending_operator);
    void fill_path(Gfx::Path const&, Gfx::PaintStyle const&, Optional<Gfx::Filter>, float global_alpha, Gfx::CompositingAndBlendingOperator compositing_and_blending_operator, Gfx::WindingRule);
    void draw_glyph_run(Gfx::GlyphRun const&, FloatPoint, PaintStyle const&, Optional<Filter>, float global_alpha, CompositingAndBlendingOperator);
    void draw_glyphs(Gfx::Font const&, float scale, ReadonlySpan<u32> glyph_ids, ReadonlySpan<FloatPoint> baselines, PaintStyle const&, Optional<Filter>, float global_alpha, CompositingAndBlendingOperator);
    void set_transform(Gfx::AffineTransform const&);
    void save();
    void restore();
    void clip(Gfx::Path const&, Gfx::WindingRule);
    void reset();

    PaintingSurface& surface() { return *m_painting_surface; }

private:
    int canvas() const { return m_painting_surface->webgfx_canvas(); }

    NonnullRefPtr<PaintingSurface> m_painting_surface;
    int m_save_depth { 0 };
};

// straight rgba floats for webgfx, with an extra alpha factor
void color_to_rgba(Color, float out[4], float alpha = 1.0f);

}
