// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "PathTrinity.h"

#if USE(TRINITY)

#include "GraphicsContextTrinity.h"
#include "WebGfx.h"
#include <numbers>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PathTrinity);

static constexpr float twoPi = 2 * std::numbers::pi_v<float>;

Ref<PathTrinity> PathTrinity::create(std::span<const PathSegment> segments)
{
    Ref path = adoptRef(*new PathTrinity);
    for (auto& segment : segments)
        path->addSegment(segment);
    return path;
}

PlatformPathPtr PathTrinity::emptyPlatformPath()
{
    static int empty = webgfx_path_new();
    return empty;
}

PathTrinity::~PathTrinity()
{
    if (m_platformPath)
        webgfx_path_free(m_platformPath);
}

PlatformPathPtr PathTrinity::platformPath() const
{
    if (!m_dirty && m_platformPath)
        return m_platformPath;
    if (m_platformPath)
        webgfx_path_free(m_platformPath);
    m_platformPath = webgfx_path_new();
    for (auto& element : m_elements) {
        auto& p = element.points;
        switch (element.type) {
        case PathElement::Type::MoveToPoint:
            webgfx_path_move_to(m_platformPath, p[0].x(), p[0].y());
            break;
        case PathElement::Type::AddLineToPoint:
            webgfx_path_line_to(m_platformPath, p[0].x(), p[0].y());
            break;
        case PathElement::Type::AddQuadCurveToPoint:
            webgfx_path_quad_to(m_platformPath, p[0].x(), p[0].y(), p[1].x(), p[1].y());
            break;
        case PathElement::Type::AddCurveToPoint:
            webgfx_path_cubic_to(m_platformPath, p[0].x(), p[0].y(), p[1].x(), p[1].y(), p[2].x(), p[2].y());
            break;
        case PathElement::Type::CloseSubpath:
            webgfx_path_close(m_platformPath);
            break;
        }
    }
    m_dirty = false;
    return m_platformPath;
}

void PathTrinity::append(PathElement::Type type, std::initializer_list<FloatPoint> points)
{
    PathElement element { type, { } };
    size_t i = 0;
    for (auto& point : points)
        element.points[i++] = point;
    m_elements.append(element);
    changed();
}

bool PathTrinity::definitelyEqual(const PathImpl& other) const
{
    RefPtr otherPath = dynamicDowncast<PathTrinity>(other);
    if (!otherPath)
        return false;
    if (otherPath.get() == this)
        return true;
    if (otherPath->m_elements.size() != m_elements.size())
        return false;
    for (size_t i = 0; i < m_elements.size(); ++i) {
        if (m_elements[i].type != otherPath->m_elements[i].type || m_elements[i].points != otherPath->m_elements[i].points)
            return false;
    }
    return true;
}

Ref<PathImpl> PathTrinity::copy() const
{
    Ref path = adoptRef(*new PathTrinity);
    path->m_elements = m_elements;
    path->m_current = m_current;
    path->m_subpathStart = m_subpathStart;
    path->m_hasCurrent = m_hasCurrent;
    return path;
}

void PathTrinity::add(PathMoveTo moveTo)
{
    append(PathElement::Type::MoveToPoint, { moveTo.point });
    m_current = m_subpathStart = moveTo.point;
    m_hasCurrent = true;
}

void PathTrinity::add(PathLineTo lineTo)
{
    if (!m_hasCurrent) {
        add(PathMoveTo { lineTo.point });
        return;
    }
    append(PathElement::Type::AddLineToPoint, { lineTo.point });
    m_current = lineTo.point;
}

void PathTrinity::add(PathQuadCurveTo quadTo)
{
    if (!m_hasCurrent)
        add(PathMoveTo { quadTo.controlPoint });
    append(PathElement::Type::AddQuadCurveToPoint, { quadTo.controlPoint, quadTo.endPoint });
    m_current = quadTo.endPoint;
}

void PathTrinity::add(PathBezierCurveTo cubicTo)
{
    if (!m_hasCurrent)
        add(PathMoveTo { cubicTo.controlPoint1 });
    append(PathElement::Type::AddCurveToPoint, { cubicTo.controlPoint1, cubicTo.controlPoint2, cubicTo.endPoint });
    m_current = cubicTo.endPoint;
}

void PathTrinity::addArc(const FloatPoint& center, float radiusX, float radiusY, float rotation, float start, float sweep)
{
    auto cosR = std::cos(rotation);
    auto sinR = std::sin(rotation);
    auto at = [&](float x, float y) {
        auto px = x * radiusX;
        auto py = y * radiusY;
        return FloatPoint { center.x() + px * cosR - py * sinR, center.y() + px * sinR + py * cosR };
    };
    auto first = at(std::cos(start), std::sin(start));
    if (m_hasCurrent)
        add(PathLineTo { first });
    else
        add(PathMoveTo { first });
    if (!sweep)
        return;
    // at most a quarter turn per cubic
    int pieces = std::max(1, static_cast<int>(std::ceil(std::abs(sweep) / (std::numbers::pi_v<float> / 2) - 0.0001f)));
    auto step = sweep / pieces;
    auto k = 4.0f / 3.0f * std::tan(step / 4);
    auto a = start;
    for (int i = 0; i < pieces; ++i) {
        auto b = a + step;
        auto ca = std::cos(a), sa = std::sin(a), cb = std::cos(b), sb = std::sin(b);
        add(PathBezierCurveTo { at(ca - k * sa, sa + k * ca), at(cb + k * sb, sb - k * cb), at(cb, sb) });
        a = b;
    }
}

// the signed sweep from start to end in the given direction
static float arcSweep(float start, float end, RotationDirection direction)
{
    if (direction == RotationDirection::Clockwise && start > end)
        end = start + (twoPi - std::fmod(start - end, twoPi));
    else if (direction == RotationDirection::Counterclockwise && start < end)
        end = start - (twoPi - std::fmod(end - start, twoPi));
    return std::clamp(end - start, -twoPi, twoPi);
}

void PathTrinity::add(PathArcTo arcTo)
{
    auto p1 = arcTo.controlPoint1;
    auto p2 = arcTo.controlPoint2;
    if (!m_hasCurrent)
        add(PathMoveTo { p1 });
    auto p0 = m_current;
    auto v1 = p0 - p1;
    auto v2 = p2 - p1;
    auto l1 = std::hypot(v1.width(), v1.height());
    auto l2 = std::hypot(v2.width(), v2.height());
    auto cross = v1.width() * v2.height() - v1.height() * v2.width();
    if (!arcTo.radius || !l1 || !l2 || std::abs(cross) < 1e-6f * l1 * l2) {
        add(PathLineTo { p1 });
        return;
    }
    FloatSize u1 { v1.width() / l1, v1.height() / l1 };
    FloatSize u2 { v2.width() / l2, v2.height() / l2 };
    auto cosTheta = std::clamp(u1.width() * u2.width() + u1.height() * u2.height(), -1.0f, 1.0f);
    auto theta = std::acos(cosTheta);
    auto tangent = arcTo.radius / std::tan(theta / 2);
    auto t1 = p1 + FloatSize { u1.width() * tangent, u1.height() * tangent };
    auto t2 = p1 + FloatSize { u2.width() * tangent, u2.height() * tangent };
    FloatSize bisector { u1.width() + u2.width(), u1.height() + u2.height() };
    auto bl = std::hypot(bisector.width(), bisector.height());
    auto reach = arcTo.radius / std::sin(theta / 2);
    FloatPoint center = p1 + FloatSize { bisector.width() / bl * reach, bisector.height() / bl * reach };
    auto start = std::atan2(t1.y() - center.y(), t1.x() - center.x());
    auto end = std::atan2(t2.y() - center.y(), t2.x() - center.x());
    auto sweep = end - start;
    if (sweep > std::numbers::pi_v<float>)
        sweep -= twoPi;
    else if (sweep < -std::numbers::pi_v<float>)
        sweep += twoPi;
    addArc(center, arcTo.radius, arcTo.radius, 0, start, sweep);
}

void PathTrinity::add(PathArc arc)
{
    addArc(arc.center, arc.radius, arc.radius, 0, arc.startAngle, arcSweep(arc.startAngle, arc.endAngle, arc.direction));
}

void PathTrinity::add(PathClosedArc closedArc)
{
    add(closedArc.arc);
    add(PathCloseSubpath());
}

void PathTrinity::add(PathEllipse ellipse)
{
    addArc(ellipse.center, ellipse.radiusX, ellipse.radiusY, ellipse.rotation, ellipse.startAngle,
        arcSweep(ellipse.startAngle, ellipse.endAngle, ellipse.direction));
}

void PathTrinity::add(PathEllipseInRect ellipseInRect)
{
    auto& r = ellipseInRect.rect;
    m_hasCurrent = false;
    addArc(r.center(), r.width() / 2, r.height() / 2, 0, 0, twoPi);
    add(PathCloseSubpath());
}

void PathTrinity::add(PathRect rect)
{
    auto& r = rect.rect;
    add(PathMoveTo { r.location() });
    add(PathLineTo { r.maxXMinYCorner() });
    add(PathLineTo { r.maxXMaxYCorner() });
    add(PathLineTo { r.minXMaxYCorner() });
    add(PathCloseSubpath());
}

void PathTrinity::add(PathRoundedRect roundedRect)
{
    if (!roundedRect.roundedRect.hasNonZeroRadii()) {
        add(PathRect { roundedRect.roundedRect.rect() });
        return;
    }
    for (auto& segment : beziersForRoundedRect(roundedRect.roundedRect))
        addSegment(segment);
}

void PathTrinity::add(PathContinuousRoundedRect continuousRoundedRect)
{
    // continuous corners are drawn as ordinary rounded ones
    add(PathRoundedRect { FloatRoundedRect { continuousRoundedRect.rect, CornerRadii { continuousRoundedRect.cornerWidth, continuousRoundedRect.cornerHeight } }, PathRoundedRect::Strategy::PreferNative });
}

void PathTrinity::add(PathCloseSubpath)
{
    if (!m_hasCurrent)
        return;
    append(PathElement::Type::CloseSubpath, { });
    m_current = m_subpathStart;
}

void PathTrinity::addPath(const PathTrinity& other, const AffineTransform& transform)
{
    for (auto element : other.m_elements) {
        for (auto& point : element.points)
            point = transform.mapPoint(point);
        m_elements.append(element);
    }
    if (other.m_hasCurrent) {
        m_current = transform.mapPoint(other.m_current);
        m_subpathStart = transform.mapPoint(other.m_subpathStart);
        m_hasCurrent = true;
    }
    changed();
}

bool PathTrinity::applyElements(const PathElementApplier& applier) const
{
    for (auto& element : m_elements)
        applier(element);
    return true;
}

bool PathTrinity::transform(const AffineTransform& matrix)
{
    for (auto& element : m_elements) {
        for (auto& point : element.points)
            point = matrix.mapPoint(point);
    }
    m_current = matrix.mapPoint(m_current);
    m_subpathStart = matrix.mapPoint(m_subpathStart);
    changed();
    return true;
}

FloatPoint PathTrinity::currentPoint() const
{
    return m_current;
}

FloatRect PathTrinity::fastBoundingRect() const
{
    FloatRect bounds = FloatRect::smallestRect();
    for (auto& element : m_elements) {
        size_t count = element.type == PathElement::Type::AddCurveToPoint ? 3
            : element.type == PathElement::Type::AddQuadCurveToPoint ? 2
            : element.type == PathElement::Type::CloseSubpath ? 0 : 1;
        for (size_t i = 0; i < count; ++i)
            bounds.extend(element.points[i]);
    }
    return bounds.isSmallest() ? FloatRect { } : bounds;
}

Vector<Vector<FloatPoint>> PathTrinity::flatten() const
{
    Vector<Vector<FloatPoint>> lines;
    FloatPoint current;
    auto begin = [&](FloatPoint point) {
        lines.append({ point });
        current = point;
    };
    auto to = [&](FloatPoint point) {
        if (lines.isEmpty())
            begin(current);
        lines.last().append(point);
        current = point;
    };
    for (auto& element : m_elements) {
        auto& p = element.points;
        switch (element.type) {
        case PathElement::Type::MoveToPoint:
            begin(p[0]);
            break;
        case PathElement::Type::AddLineToPoint:
            to(p[0]);
            break;
        case PathElement::Type::AddQuadCurveToPoint: {
            auto p0 = current;
            for (int i = 1; i <= 8; ++i) {
                float t = i / 8.0f, u = 1 - t;
                to({ u * u * p0.x() + 2 * u * t * p[0].x() + t * t * p[1].x(), u * u * p0.y() + 2 * u * t * p[0].y() + t * t * p[1].y() });
            }
            break;
        }
        case PathElement::Type::AddCurveToPoint: {
            auto p0 = current;
            for (int i = 1; i <= 16; ++i) {
                float t = i / 16.0f, u = 1 - t;
                float a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
                to({ a * p0.x() + b * p[0].x() + c * p[1].x() + d * p[2].x(), a * p0.y() + b * p[0].y() + c * p[1].y() + d * p[2].y() });
            }
            break;
        }
        case PathElement::Type::CloseSubpath:
            if (!lines.isEmpty() && !lines.last().isEmpty()) {
                auto first = lines.last().first();
                to(first);
                begin(first);
            }
            break;
        }
    }
    return lines;
}

FloatRect PathTrinity::boundingRect() const
{
    FloatRect bounds = FloatRect::smallestRect();
    for (auto& line : flatten()) {
        for (auto& point : line)
            bounds.extend(point);
    }
    return bounds.isSmallest() ? FloatRect { } : bounds;
}

bool PathTrinity::contains(const FloatPoint& point, WindRule windRule) const
{
    if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
        return false;
    return webgfx_path_contains(platformPath(), point.x(), point.y(), windRule == WindRule::EvenOdd);
}

// half the stroke's width, grown for miters and square caps
static float strokeReach(const GraphicsContextTrinity& context)
{
    auto half = context.strokeThickness() / 2;
    if (context.lineJoin() == LineJoin::Miter)
        return half * std::max(1.0f, context.miterLimit());
    if (context.lineCap() == LineCap::Square)
        return half * std::numbers::sqrt2_v<float>;
    return half;
}

bool PathTrinity::strokeContains(const FloatPoint& point, NOESCAPE const Function<void(GraphicsContext&)>& strokeStyleApplier) const
{
    if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
        return false;
    GraphicsContextTrinity context(0, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified);
    strokeStyleApplier(context);
    auto half = context.strokeThickness() / 2;
    for (auto& line : flatten()) {
        for (size_t i = 1; i < line.size(); ++i) {
            auto a = line[i - 1], b = line[i];
            auto ab = b - a;
            auto length2 = ab.width() * ab.width() + ab.height() * ab.height();
            auto t = length2 ? std::clamp(((point.x() - a.x()) * ab.width() + (point.y() - a.y()) * ab.height()) / length2, 0.0f, 1.0f) : 0.0f;
            auto dx = point.x() - (a.x() + t * ab.width());
            auto dy = point.y() - (a.y() + t * ab.height());
            if (dx * dx + dy * dy <= half * half)
                return true;
        }
    }
    return false;
}

FloatRect PathTrinity::strokeBoundingRect(NOESCAPE const Function<void(GraphicsContext&)>& strokeStyleApplier) const
{
    GraphicsContextTrinity context(0, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified);
    strokeStyleApplier(context);
    auto bounds = boundingRect();
    bounds.inflate(strokeReach(context));
    return bounds;
}

} // namespace WebCore

#endif // USE(TRINITY)
