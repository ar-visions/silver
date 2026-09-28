// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#if USE(TRINITY)

#include "PathElement.h"
#include "PathImpl.h"
#include "PlatformPath.h"
#include "WindRule.h"
#include <wtf/Function.h>
#include <wtf/Vector.h>

namespace WebCore {

class GraphicsContext;

// WebKit's own elements are the path; a webgfx path is built
// from them when trinity fills, hit-tests or measures it
class PathTrinity final : public PathImpl {
    WTF_MAKE_TZONE_ALLOCATED(PathTrinity);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PathTrinity);
public:
    WEBCORE_EXPORT static Ref<PathTrinity> create(std::span<const PathSegment> = { });
    static PlatformPathPtr emptyPlatformPath();
    ~PathTrinity();

    // the webgfx path for these elements, rebuilt on change
    PlatformPathPtr platformPath() const;

    void addPath(const PathTrinity&, const AffineTransform&);

    bool definitelyEqual(const PathImpl&) const final;
    Ref<PathImpl> copy() const final;
    void add(PathMoveTo) final;
    void add(PathLineTo) final;
    void add(PathQuadCurveTo) final;
    void add(PathBezierCurveTo) final;
    void add(PathArcTo) final;
    void add(PathArc) final;
    void add(PathClosedArc) final;
    void add(PathEllipse) final;
    void add(PathEllipseInRect) final;
    void add(PathRect) final;
    void add(PathRoundedRect) final;
    void add(PathContinuousRoundedRect) final;
    void add(PathCloseSubpath) final;

    bool applyElements(const PathElementApplier&) const final;
    bool transform(const AffineTransform&) final;

    bool contains(const FloatPoint&, WindRule) const;
    bool strokeContains(const FloatPoint&, NOESCAPE const Function<void(GraphicsContext&)>& strokeStyleApplier) const;
    FloatRect strokeBoundingRect(NOESCAPE const Function<void(GraphicsContext&)>& strokeStyleApplier) const;

    const Vector<PathElement>& elements() const { return m_elements; }

private:
    PathTrinity() = default;

    FloatPoint currentPoint() const final;
    FloatRect fastBoundingRect() const final;
    FloatRect boundingRect() const final;

    void append(PathElement::Type, std::initializer_list<FloatPoint>);
    // an elliptical arc as cubics; sweep is signed radians
    void addArc(const FloatPoint& center, float radiusX, float radiusY, float rotation, float start, float sweep);
    void changed() { m_dirty = true; }
    // the flattened outline, polylines per subpath
    Vector<Vector<FloatPoint>> flatten() const;

    Vector<PathElement> m_elements;
    FloatPoint m_current;
    FloatPoint m_subpathStart;
    bool m_hasCurrent { false };
    mutable int m_platformPath { 0 };
    mutable bool m_dirty { true };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::PathTrinity)
    static bool isType(const WebCore::PathImpl& pathImpl) { return !pathImpl.isPathStream(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // USE(TRINITY)
