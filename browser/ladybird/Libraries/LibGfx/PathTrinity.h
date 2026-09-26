/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/OwnPtr.h>
#include <LibGfx/Path.h>

namespace Gfx {

// a trinity CanvasPath, held by its webgfx id
class PathImplTrinity final : public PathImpl {
public:
    static NonnullOwnPtr<Gfx::PathImplTrinity> create();

    virtual ~PathImplTrinity() override;

    virtual void clear() override;
    virtual void move_to(Gfx::FloatPoint const&) override;
    virtual void line_to(Gfx::FloatPoint const&) override;
    virtual void close() override;
    virtual void elliptical_arc_to(FloatPoint point, FloatSize radii, float x_axis_rotation, bool large_arc, bool sweep) override;
    virtual void arc_to(FloatPoint point, float radius, bool large_arc, bool sweep) override;
    virtual void quadratic_bezier_curve_to(FloatPoint through, FloatPoint point) override;
    virtual void cubic_bezier_curve_to(FloatPoint c1, FloatPoint c2, FloatPoint p2) override;
    virtual void glyph_run(GlyphRun const&) override;
    virtual void offset(Gfx::FloatPoint const&) override;

    virtual void append_path(Gfx::Path const&) override;
    virtual void intersect(Gfx::Path const&) override;

    [[nodiscard]] virtual Vector<u8> serialize_to_bytes() const override;
    virtual void deserialize_from_bytes(ReadonlyBytes) override;

    [[nodiscard]] virtual bool is_empty() const override;
    virtual Gfx::FloatPoint last_point() const override;
    virtual Gfx::FloatRect bounding_box() const override;
    virtual float length() const override;
    virtual bool contains(FloatPoint point, Gfx::WindingRule) const override;
    virtual void set_fill_type(Gfx::WindingRule winding_rule) override;

    virtual NonnullOwnPtr<PathImpl> clone() const override;
    virtual NonnullOwnPtr<PathImpl> copy_transformed(Gfx::AffineTransform const&) const override;
    virtual NonnullOwnPtr<PathImpl> place_glyph_runs_along(ReadonlySpan<NonnullRefPtr<GlyphRun>>, float offset = 0) const override;

    virtual String to_svg_string() const override;

    int webgfx_path() const { return m_path; }
    Gfx::WindingRule fill_type() const { return m_fill_type; }

private:
    PathImplTrinity();
    PathImplTrinity(PathImplTrinity const& other);

    void append_points_of(int path);

    int m_path { 0 };
    Gfx::FloatPoint m_last_move_to;
    bool m_has_current_point { false };
    Gfx::WindingRule m_fill_type { Gfx::WindingRule::Nonzero };
};

}
