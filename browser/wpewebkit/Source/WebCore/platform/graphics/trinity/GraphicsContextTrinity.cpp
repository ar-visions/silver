// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "GraphicsContextTrinity.h"

#if USE(TRINITY)

#include "FloatRoundedRect.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include "Path.h"
#include "Pattern.h"
#include "TrinityImage.h"
#include "VideoFrameTrinity.h"
#include "WebGfx.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

void trinityNotPorted(ASCIILiteral what)
{
    static Lock lock;
    static NeverDestroyed<HashSet<const char*>> seen;
    Locker locker { lock };
    if (seen->add(what.characters()).isNewEntry)
        WTFLogAlways("trinity painter: not ported: %s", what.characters());
}

static void toRGBA(const Color& color, float out[4])
{
    auto [r, g, b, a] = color.toColorTypeLossy<SRGBA<float>>().resolved();
    out[0] = r;
    out[1] = g;
    out[2] = b;
    out[3] = a;
}

// no canvas: clips are unbounded
static IntRect wholeSpace(IntSize size)
{
    if (!size.isEmpty())
        return { { }, size };
    constexpr int reach = 1 << 28;
    return { -reach, -reach, 2 * reach, 2 * reach };
}

GraphicsContextTrinity::GraphicsContextTrinity(int canvas, RenderingMode renderingMode, RenderingPurpose renderingPurpose, IntSize size)
    : m_canvas(canvas)
    , m_size(size)
    , m_renderingMode(renderingMode)
    , m_renderingPurpose(renderingPurpose)
    , m_clip(wholeSpace(size))
{
    webgfx_canvas_save(m_canvas);
    applyTransform();
}

GraphicsContextTrinity::~GraphicsContextTrinity()
{
    while (!m_frames.isEmpty())
        restore(m_state.purpose());
    webgfx_canvas_restore(m_canvas);
    if (!m_releaseBacking)
        return;
    flushToBacking();
    webgfx_canvas_free(m_canvas);
    m_releaseBacking();
}

// the memory's pixels become the canvas's starting content
void GraphicsContextTrinity::attachBacking(std::span<uint8_t> pixels, unsigned bytesPerRow, Function<void()>&& release)
{
    m_backing = pixels;
    m_backingBytesPerRow = bytesPerRow;
    m_releaseBacking = WTF::move(release);
    loadFromBacking();
}

void GraphicsContextTrinity::loadFromBacking()
{
    auto pixels = m_backing;
    auto bytesPerRow = m_backingBytesPerRow;
    int width = m_size.width(), height = m_size.height();
    Vector<uint8_t> rgba(width * height * 4);
    for (int y = 0; y < height; ++y) {
        auto* from = pixels.data() + y * bytesPerRow;
        auto* to = rgba.mutableSpan().data() + y * width * 4;
        for (int x = 0; x < width; ++x) {
            uint8_t a = from[x * 4 + 3];
            auto straight = [a](uint8_t c) -> uint8_t {
                return a && a != 255 ? std::min(255, c * 255 / a) : c;
            };
            to[x * 4 + 0] = straight(from[x * 4 + 2]);
            to[x * 4 + 1] = straight(from[x * 4 + 1]);
            to[x * 4 + 2] = straight(from[x * 4 + 0]);
            to[x * 4 + 3] = a;
        }
    }
    float const identity[6] = { 1, 0, 0, 1, 0, 0 };
    float const dst[4] = { 0, 0, static_cast<float>(width), static_cast<float>(height) };
    float const uv[4] = { 0, 0, 1, 1 };
    float const clear[4] = { 0, 0, 0, 0 };
    webgfx_canvas_clear(m_canvas, clear);
    webgfx_canvas_save(m_canvas);
    webgfx_canvas_transform(m_canvas, identity);
    webgfx_canvas_image(m_canvas, rgba.span().data(), width, height, dst, uv);
    webgfx_canvas_restore(m_canvas);
}

void GraphicsContextTrinity::flushToBacking()
{
    if (m_backing.empty())
        return;
    int width = m_size.width(), height = m_size.height();
    Vector<uint8_t> rgba(width * height * 4);
    webgfx_canvas_read(m_canvas, rgba.mutableSpan().data());
    for (int y = 0; y < height; ++y) {
        auto* from = rgba.span().data() + y * width * 4;
        auto* to = m_backing.data() + y * m_backingBytesPerRow;
        for (int x = 0; x < width; ++x) {
            to[x * 4 + 0] = from[x * 4 + 2];
            to[x * 4 + 1] = from[x * 4 + 1];
            to[x * 4 + 2] = from[x * 4 + 0];
            to[x * 4 + 3] = from[x * 4 + 3];
        }
    }
}

void GraphicsContextTrinity::didUpdateState(GraphicsContextState&)
{
}

Color GraphicsContextTrinity::withAlpha(const Color& color) const
{
    return color.colorWithAlphaMultipliedBy(alpha());
}

void GraphicsContextTrinity::applyTransform()
{
    applyTransformTo(m_canvas);
}

void GraphicsContextTrinity::applyTransformTo(int canvas)
{
    float const m[6] = { static_cast<float>(m_ctm.a()), static_cast<float>(m_ctm.b()), static_cast<float>(m_ctm.c()),
        static_cast<float>(m_ctm.d()), static_cast<float>(m_ctm.e()), static_cast<float>(m_ctm.f()) };
    webgfx_canvas_transform(canvas, m);
}

void GraphicsContextTrinity::save(GraphicsContextState::Purpose purpose)
{
    GraphicsContext::save(purpose);
    m_frames.append({ m_ctm, m_clip, m_stroke });
    webgfx_canvas_save(m_canvas);
}

void GraphicsContextTrinity::restore(GraphicsContextState::Purpose purpose)
{
    if (m_frames.isEmpty())
        return;
    GraphicsContext::restore(purpose);
    auto frame = m_frames.takeLast();
    m_ctm = frame.ctm;
    m_clip = frame.clip;
    m_stroke = frame.stroke;
    webgfx_canvas_restore(m_canvas);
}

void GraphicsContextTrinity::translate(float x, float y)
{
    m_ctm.translate(x, y);
    applyTransform();
}

void GraphicsContextTrinity::rotate(float radians)
{
    m_ctm.rotateRadians(radians);
    applyTransform();
}

void GraphicsContextTrinity::scale(const FloatSize& size)
{
    m_ctm.scale(size);
    applyTransform();
}

void GraphicsContextTrinity::concatCTM(const AffineTransform& transform)
{
    m_ctm.multiply(transform);
    applyTransform();
}

void GraphicsContextTrinity::setCTM(const AffineTransform& transform)
{
    m_ctm = transform;
    applyTransform();
}

AffineTransform GraphicsContextTrinity::getCTM(IncludeDeviceScale) const
{
    return m_ctm;
}

void GraphicsContextTrinity::setLineCap(LineCap cap)
{
    m_stroke.cap = cap;
}

void GraphicsContextTrinity::setLineDash(const DashArray& dashes, float)
{
    if (!dashes.isEmpty())
        trinityNotPorted("dashed strokes"_s);
}

void GraphicsContextTrinity::setLineJoin(LineJoin join)
{
    m_stroke.join = join;
}

void GraphicsContextTrinity::setMiterLimit(float miter)
{
    m_stroke.miter = miter;
}

void GraphicsContextTrinity::clipDevice(const IntRect& rect)
{
    m_clip.intersect(rect);
    webgfx_canvas_clip(m_canvas, rect.x(), rect.y(), rect.width(), rect.height());
}

void GraphicsContextTrinity::clip(const FloatRect& rect)
{
    if (!m_ctm.preservesAxisAlignment())
        trinityNotPorted("clips under rotation (bounds used)"_s);
    clipDevice(enclosingIntRect(m_ctm.mapRect(rect)));
}

// offscreen canvases the size of one context, reused
static Lock layerPoolLock;

static Vector<std::pair<IntSize, int>>& layerPool() WTF_REQUIRES_LOCK(layerPoolLock)
{
    static NeverDestroyed<Vector<std::pair<IntSize, int>>> pool;
    return pool;
}

static int takeLayerCanvas(IntSize size)
{
    {
        Locker locker { layerPoolLock };
        auto& pool = layerPool();
        for (size_t i = 0; i < pool.size(); ++i) {
            if (pool[i].first == size) {
                int canvas = pool[i].second;
                pool.removeAt(i);
                return canvas;
            }
        }
    }
    return webgfx_canvas_new(size.width(), size.height());
}

static void giveLayerCanvas(IntSize size, int canvas)
{
    if (!canvas)
        return;
    Locker locker { layerPoolLock };
    layerPool().append({ size, canvas });
}

void GraphicsContextTrinity::clipPath(const Path& path, WindRule windRule)
{
    clipDevice(enclosingIntRect(m_ctm.mapRect(path.fastBoundingRect())));
    if (!path.isEmpty())
        webgfx_canvas_clip_path(m_canvas, path.platformPath(), windRule == WindRule::EvenOdd);
}

void GraphicsContextTrinity::clipRoundedRect(const FloatRoundedRect& rect)
{
    auto& radii = rect.radii();
    // elliptical corners take the path clip
    if (radii.topLeft().width() != radii.topLeft().height() || radii.topRight().width() != radii.topRight().height()
        || radii.bottomRight().width() != radii.bottomRight().height() || radii.bottomLeft().width() != radii.bottomLeft().height()) {
        GraphicsContext::clipRoundedRect(rect);
        return;
    }
    clipDevice(enclosingIntRect(m_ctm.mapRect(rect.rect())));
    float const corners[4] = { radii.topLeft().width(), radii.topRight().width(), radii.bottomRight().width(), radii.bottomLeft().width() };
    auto& box = rect.rect();
    webgfx_canvas_clip_rounded_rect(m_canvas, box.x(), box.y(), box.width(), box.height(), corners);
}

void GraphicsContextTrinity::clipOut(const Path& path)
{
    if (!path.isEmpty())
        webgfx_canvas_clip_out_path(m_canvas, path.platformPath());
}

void GraphicsContextTrinity::clipOut(const FloatRect& rect)
{
    webgfx_canvas_clip_out_rect(m_canvas, rect.x(), rect.y(), rect.width(), rect.height());
}

void GraphicsContextTrinity::resetClip()
{
    trinityNotPorted("reset clip"_s);
}

IntRect GraphicsContextTrinity::clipBounds() const
{
    auto inverse = m_ctm.inverse();
    if (!inverse)
        return { };
    return enclosingIntRect(inverse->mapRect(FloatRect(m_clip)));
}

// the image's alpha limits later draws, over rect
void GraphicsContextTrinity::clipToImageBuffer(ImageBuffer& buffer, const FloatRect& rect)
{
    clip(rect);
    auto image = buffer.copyNativeImage();
    if (!image || !image->platformImage())
        return;
    float const dst[4] = { rect.x(), rect.y(), rect.width(), rect.height() };
    webgfx_canvas_clip_image(m_canvas, image->platformImage()->webgfxImage(), dst);
}

// later draws land on a clear layer with the same transform;
// the canvas below keeps the clips for the layer's end
void GraphicsContextTrinity::pushLayer(float opacity, CompositeOperator operation)
{
    Layer layer { 0, m_canvas, opacity, operation };
    if (m_canvas && !m_size.isEmpty())
        layer.canvas = takeLayerCanvas(m_size);
    m_layers.append(layer);
    if (!layer.canvas)
        return;
    m_canvas = layer.canvas;
    float const clear[4] = { 0, 0, 0, 0 };
    webgfx_canvas_clear(m_canvas, clear);
    webgfx_canvas_save(m_canvas);
    applyTransform();
    // the layer's opacity and mode apply once, at its end
    setCompositeOperation(CompositeOperator::SourceOver);
    setAlpha(1);
}

void GraphicsContextTrinity::popLayer()
{
    if (m_layers.isEmpty())
        return;
    auto layer = m_layers.takeLast();
    if (!layer.canvas)
        return;
    webgfx_canvas_restore(layer.canvas);
    m_canvas = layer.parent;
    webgfx_canvas_set_composite(m_canvas, static_cast<int>(layer.operation));
    webgfx_canvas_compose(m_canvas, layer.canvas, 0, false, layer.opacity, 0, 0, m_size.width(), m_size.height());
    webgfx_canvas_set_composite(m_canvas, static_cast<int>(CompositeOperator::SourceOver));
    giveLayerCanvas(m_size, layer.canvas);
}

void GraphicsContextTrinity::beginTransparencyLayer(float opacity)
{
    GraphicsContext::beginTransparencyLayer(opacity);
    auto operation = compositeOperation();
    save(GraphicsContextState::Purpose::TransparencyLayer);
    pushLayer(opacity, operation);
}

void GraphicsContextTrinity::beginTransparencyLayer(CompositeOperator operation, BlendMode blendMode)
{
    GraphicsContext::beginTransparencyLayer(operation, blendMode);
    if (blendMode != BlendMode::Normal)
        trinityNotPorted("layer blend modes"_s);
    auto opacity = alpha();
    save(GraphicsContextState::Purpose::TransparencyLayer);
    pushLayer(opacity, operation);
}

void GraphicsContextTrinity::endTransparencyLayer()
{
    GraphicsContext::endTransparencyLayer();
    popLayer();
    restore(GraphicsContextState::Purpose::TransparencyLayer);
}

// the state's drop shadow into canvas shadow slot 0
void GraphicsContextTrinity::setShadowForDraw()
{
    auto shadow = dropShadow();
    if (!shadow || !shadow->isVisible()) {
        webgfx_canvas_clear_shadows(m_canvas);
        return;
    }
    float blur = shadow->radiusMode == ShadowRadiusMode::Legacy ? shadow->radius * 2 : shadow->radius;
    FloatSize offset = shadow->offset;
    if (shadowsIgnoreTransforms()) {
        // offset and blur are device pixels: into user space
        float sx = std::max(static_cast<float>(m_ctm.xScale()), 0.0001f);
        float sy = std::max(static_cast<float>(m_ctm.yScale()), 0.0001f);
        offset.scale(1 / sx, 1 / sy);
        blur /= sx;
    }
    float rgba[4];
    toRGBA(shadow->color.colorWithAlphaMultipliedBy(shadow->opacity), rgba);
    webgfx_canvas_set_shadow(m_canvas, 0, rgba, offset.width(), offset.height(), blur, 0, false);
}

void GraphicsContextTrinity::fillColorRect(const FloatRect& rect, const Color& color)
{
    setShadowForDraw();
    float rgba[4];
    toRGBA(withAlpha(color), rgba);
    webgfx_canvas_fill_rect(m_canvas, rect.x(), rect.y(), rect.width(), rect.height(), rgba, nullptr);
    webgfx_canvas_clear_shadows(m_canvas);
}

// 256 texels of the stops, blended premultiplied, stored straight
static Vector<uint8_t> gradientRamp(const Gradient& gradient)
{
    auto& stops = gradient.stops().sorted().stops();
    Vector<uint8_t> ramp(256 * 4);
    if (stops.isEmpty())
        return ramp;
    auto premultiplied = [](const Color& color) {
        auto [r, g, b, a] = color.toColorTypeLossy<SRGBA<float>>().resolved();
        return std::array<float, 4> { r * a, g * a, b * a, a };
    };
    size_t next = 0;
    for (int i = 0; i < 256; ++i) {
        float t = i / 255.0f;
        while (next < stops.size() && stops[next].offset <= t)
            ++next;
        std::array<float, 4> c;
        if (!next)
            c = premultiplied(stops.first().color);
        else if (next == stops.size())
            c = premultiplied(stops.last().color);
        else {
            auto& a = stops[next - 1];
            auto& b = stops[next];
            float span = b.offset - a.offset;
            float f = span > 0 ? (t - a.offset) / span : 1;
            auto ca = premultiplied(a.color), cb = premultiplied(b.color);
            for (int k = 0; k < 4; ++k)
                c[k] = ca[k] + (cb[k] - ca[k]) * f;
        }
        float alpha = c[3];
        for (int k = 0; k < 3; ++k)
            ramp[i * 4 + k] = static_cast<uint8_t>(std::clamp(alpha > 0 ? c[k] / alpha : 0.0f, 0.0f, 1.0f) * 255 + 0.5f);
        ramp[i * 4 + 3] = static_cast<uint8_t>(std::clamp(alpha, 0.0f, 1.0f) * 255 + 0.5f);
    }
    return ramp;
}

// one webgfx image per distinct ramp, kept for the process
static int rampImage(const Vector<uint8_t>& ramp)
{
    static Lock lock;
    static NeverDestroyed<HashMap<unsigned, int>> images;
    unsigned key = computeHash(ramp.span());
    Locker locker { lock };
    return images->ensure(key, [&] {
        return webgfx_image_new(ramp.span().data(), 256, 1);
    }).iterator->value;
}

bool GraphicsContextTrinity::setGradient(const Gradient& gradient, const AffineTransform& gradientSpaceTransform)
{
    // conic and elliptical forms are folded into the matrix
    AffineTransform space = gradientSpaceTransform;
    int kind = 1;
    float pts[4] = { 0, 0, 0, 0 };
    float r0 = 0, r1 = 0;
    WTF::switchOn(gradient.data(),
        [&](const Gradient::LinearData& data) {
            pts[0] = data.point0.x();
            pts[1] = data.point0.y();
            pts[2] = data.point1.x();
            pts[3] = data.point1.y();
        },
        [&](const Gradient::RadialData& data) {
            kind = 2;
            if (data.aspectRatio != 1) {
                space.translate(data.point0.x(), data.point0.y());
                space.scale(1, 1 / data.aspectRatio);
                space.translate(-data.point0.x(), -data.point0.y());
            }
            pts[0] = data.point0.x();
            pts[1] = data.point0.y();
            pts[2] = data.point1.x();
            pts[3] = data.point1.y();
            r0 = std::max(data.startRadius, 0.0f);
            r1 = std::max(data.endRadius, 0.0f);
        },
        [&](const Gradient::ConicData& data) {
            kind = 3;
            // css 0 is up, the shader's 0 is along +x
            space.translate(data.point0.x(), data.point0.y());
            space.rotateRadians(data.angleRadians - piOverTwoFloat);
            space.translate(-data.point0.x(), -data.point0.y());
            pts[0] = data.point0.x();
            pts[1] = data.point0.y();
        });
    auto inverse = (m_ctm * space).inverse();
    if (!inverse)
        return false;
    float const m[6] = { static_cast<float>(inverse->a()), static_cast<float>(inverse->c()), static_cast<float>(inverse->e()),
        static_cast<float>(inverse->b()), static_cast<float>(inverse->d()), static_cast<float>(inverse->f()) };
    int spread = gradient.spreadMethod() == GradientSpreadMethod::Reflect ? 1 : gradient.spreadMethod() == GradientSpreadMethod::Repeat ? 2 : 0;
    webgfx_canvas_set_ramp(m_canvas, rampImage(gradientRamp(gradient)), kind, spread, alpha(), m, pts, r0, r1);
    return true;
}

void GraphicsContextTrinity::clearGradient()
{
    float const zero[6] = { };
    webgfx_canvas_set_ramp(m_canvas, 0, 0, 0, 0, zero, zero, 0, 0);
}

// colors, and gradients through the stops ramp
void GraphicsContextTrinity::fillWithBrush(const Function<void(const Color&)>& fill)
{
    if (auto* gradient = fillGradient()) {
        if (setGradient(*gradient, fillGradientSpaceTransform())) {
            fill(Color::white);
            clearGradient();
        }
        return;
    }
    if (fillPattern()) {
        trinityNotPorted("pattern fills of shapes"_s);
        return;
    }
    fill(fillColor());
}

void GraphicsContextTrinity::fillRect(const FloatRect& rect, RequiresClipToRect)
{
    if (auto* pattern = fillPattern()) {
        auto image = pattern->tileNativeImage();
        if (!image)
            return;
        auto size = FloatSize(image->size());
        auto repeatX = pattern->repeatX() ? 0.0f : std::numeric_limits<float>::max();
        auto repeatY = pattern->repeatY() ? 0.0f : std::numeric_limits<float>::max();
        drawPattern(*image, rect, { { }, size }, pattern->patternSpaceTransform(), { }, { repeatX, repeatY }, { });
        return;
    }
    fillWithBrush([&](const Color& color) {
        fillColorRect(rect, color);
    });
}

void GraphicsContextTrinity::fillRect(const FloatRect& rect, const Color& color)
{
    fillColorRect(rect, color);
}

void GraphicsContextTrinity::fillRect(const FloatRect& rect, Gradient& gradient, const AffineTransform& gradientSpaceTransform, RequiresClipToRect)
{
    if (!setGradient(gradient, gradientSpaceTransform))
        return;
    fillColorRect(rect, Color::white);
    clearGradient();
}

void GraphicsContextTrinity::fillRoundedRectImpl(const FloatRoundedRect& rect, const Color& color)
{
    setShadowForDraw();
    auto& radii = rect.radii();
    if (radii.topLeft().width() != radii.topLeft().height() || radii.bottomRight().width() != radii.bottomRight().height())
        trinityNotPorted("elliptical corners (width used)"_s);
    float rgba[4];
    toRGBA(withAlpha(color), rgba);
    float const corners[4] = { radii.topLeft().width(), radii.topRight().width(), radii.bottomRight().width(), radii.bottomLeft().width() };
    auto& box = rect.rect();
    webgfx_canvas_fill_rect(m_canvas, box.x(), box.y(), box.width(), box.height(), rgba, corners);
    webgfx_canvas_clear_shadows(m_canvas);
}

void GraphicsContextTrinity::fillPath(const Path& path)
{
    if (path.isEmpty())
        return;
    setShadowForDraw();
    fillWithBrush([&](const Color& color) {
        float rgba[4];
        toRGBA(withAlpha(color), rgba);
        webgfx_canvas_fill_path(m_canvas, path.platformPath(), rgba, fillRule() == WindRule::EvenOdd);
    });
    webgfx_canvas_clear_shadows(m_canvas);
}

void GraphicsContextTrinity::strokePath(const Path& path)
{
    if (path.isEmpty() || strokeStyle() == StrokeStyle::NoStroke)
        return;
    if (strokeGradient() || strokePattern()) {
        trinityNotPorted("gradient and pattern strokes"_s);
        return;
    }
    float rgba[4];
    toRGBA(withAlpha(strokeColor()), rgba);
    int cap = lineCap() == LineCap::Round ? 1 : lineCap() == LineCap::Square ? 2 : 0;
    webgfx_canvas_stroke_path(m_canvas, path.platformPath(), rgba, strokeThickness(), cap);
}

void GraphicsContextTrinity::strokeRect(const FloatRect& rect, float lineWidth)
{
    auto thickness = strokeThickness();
    setStrokeThickness(lineWidth);
    Path path;
    path.addRect(rect);
    strokePath(path);
    setStrokeThickness(thickness);
}

void GraphicsContextTrinity::clearRect(const FloatRect& rect)
{
    auto device = enclosingIntRect(m_ctm.mapRect(rect));
    if (device.contains(IntRect({ }, m_size))) {
        float const clear[4] = { 0, 0, 0, 0 };
        webgfx_canvas_clear(m_canvas, clear);
        return;
    }
    // part of it: copy transparent over the rect
    float const none[4] = { 0, 0, 0, 0 };
    webgfx_canvas_save(m_canvas);
    webgfx_canvas_set_composite(m_canvas, static_cast<int>(CompositeOperator::Copy));
    webgfx_canvas_fill_rect(m_canvas, rect.x(), rect.y(), rect.width(), rect.height(), none, nullptr);
    webgfx_canvas_restore(m_canvas);
}

void GraphicsContextTrinity::drawRect(const FloatRect& rect, float borderThickness)
{
    fillRect(rect);
    if (strokeStyle() == StrokeStyle::NoStroke)
        return;
    auto color = strokeColor();
    fillColorRect({ rect.x(), rect.y(), rect.width(), borderThickness }, color);
    fillColorRect({ rect.x(), rect.maxY() - borderThickness, rect.width(), borderThickness }, color);
    fillColorRect({ rect.x(), rect.y() + borderThickness, borderThickness, rect.height() - 2 * borderThickness }, color);
    fillColorRect({ rect.maxX() - borderThickness, rect.y() + borderThickness, borderThickness, rect.height() - 2 * borderThickness }, color);
}

// borders only: level or upright lines, thickness across
void GraphicsContextTrinity::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    if (strokeStyle() == StrokeStyle::NoStroke)
        return;
    auto thickness = strokeThickness();
    bool isVertical = point1.x() + thickness == point2.x();
    float length = isVertical ? point2.y() - point1.y() : point2.x() - point1.x();
    if (!thickness || !length)
        return;
    auto color = strokeColor();
    auto segment = [&](float from, float size) {
        if (isVertical)
            fillColorRect({ point1.x(), point1.y() + from, thickness, size }, color);
        else
            fillColorRect({ point1.x() + from, point1.y(), size, thickness }, color);
    };
    if (strokeStyle() != StrokeStyle::DottedStroke && strokeStyle() != StrokeStyle::DashedStroke) {
        segment(0, length);
        return;
    }
    float corner = dashedLineCornerWidthForStrokeWidth(length);
    segment(0, corner);
    segment(length - corner, corner);
    float inner = length - 2 * corner;
    float pattern = dashedLinePatternWidthForStrokeWidth(inner);
    if (inner <= pattern + 1)
        return;
    // dashes start after a gap of `offset` past the corner
    float offset = dashedLinePatternOffsetForPatternAndStrokeWidth(pattern, inner);
    for (float at = pattern * 2 - offset; at < inner; at += pattern * 2) {
        float from = std::max(at, 0.0f);
        float to = std::min(at + pattern, inner);
        if (to > from)
            segment(corner + from, to - from);
    }
}

void GraphicsContextTrinity::drawEllipse(const FloatRect& rect)
{
    Path path;
    path.addEllipseInRect(rect);
    fillPath(path);
}

void GraphicsContextTrinity::drawLinesForText(const FloatPoint& point, float thickness, std::span<const FloatSegment> lineSegments, bool printing, bool doubleLines, StrokeStyle strokeStyle)
{
    auto [rects, color] = computeRectsAndStrokeColorForLinesForText(point, thickness, lineSegments, printing, doubleLines, strokeStyle);
    for (auto& rect : rects)
        fillColorRect(rect, color);
}

void GraphicsContextTrinity::drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle)
{
    trinityNotPorted("spelling and grammar marks"_s);
}

void GraphicsContextTrinity::drawFocusRing(const Path& path, float outlineWidth, const Color& color, float)
{
    auto thickness = strokeThickness();
    auto stroke = strokeColor();
    setStrokeThickness(outlineWidth);
    setStrokeColor(color);
    strokePath(path);
    setStrokeThickness(thickness);
    setStrokeColor(stroke);
}

void GraphicsContextTrinity::drawFocusRing(const Vector<FloatRect>& rects, float outlineWidth, const Color& color, float zoomFactor)
{
    Path path;
    for (auto& rect : rects)
        path.addRect(rect);
    drawFocusRing(path, outlineWidth, color, zoomFactor);
}

void GraphicsContextTrinity::drawNativeImage(const NativeImage& nativeImage, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    auto& image = nativeImage.platformImage();
    if (!image)
        return;
    if (alpha() < 1)
        trinityNotPorted("image alpha"_s);
    if (options.compositeOperator() != CompositeOperator::SourceOver || options.blendMode() != BlendMode::Normal)
        trinityNotPorted("image blend modes"_s);
    auto size = FloatSize(image->size());
    auto source = normalizeRect(srcRect);
    auto dest = normalizeRect(destRect);
    bool turned = options.orientation() != ImageOrientation::Orientation::None;
    if (turned) {
        save();
        translate(dest.x(), dest.y());
        dest.setLocation({ });
        concatCTM(options.orientation().transformFromDefault(dest.size()));
        if (options.orientation().usesWidthAsHeight())
            dest.setSize(dest.size().transposedSize());
    }
    float const dst[4] = { dest.x(), dest.y(), dest.width(), dest.height() };
    float const uv[4] = { source.x() / size.width(), source.y() / size.height(), source.maxX() / size.width(), source.maxY() / size.height() };
    webgfx_canvas_draw_image(m_canvas, image->webgfxImage(), dst, uv);
    if (turned)
        restore();
}

#if ENABLE(VIDEO)
// a trinity frame draws from its gpu planes; others as images
void GraphicsContextTrinity::drawVideoFrame(const VideoFrame& frame, const FloatRect& destination, ImageOrientation orientation, bool shouldDiscardAlpha)
{
    auto* trinity = dynamicDowncast<VideoFrameTrinity>(frame);
    if (!trinity) {
        GraphicsContext::drawVideoFrame(frame, destination, orientation, shouldDiscardAlpha);
        return;
    }
    auto dest = normalizeRect(destination);
    float const dst[4] = { dest.x(), dest.y(), dest.width(), dest.height() };
    webgfx_canvas_draw_video(m_canvas, trinity->video(), dst);
}
#endif

// tiles in pattern space, placed by transform and phase
void GraphicsContextTrinity::drawPattern(const NativeImage& nativeImage, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions)
{
    auto& image = nativeImage.platformImage();
    if (!image || !patternTransform.isInvertible() || tileRect.isEmpty())
        return;
    if (patternTransform.b() || patternTransform.c()) {
        trinityNotPorted("rotated patterns"_s);
        return;
    }
    auto size = FloatSize(image->size());
    float const uv[4] = { tileRect.x() / size.width(), tileRect.y() / size.height(), tileRect.maxX() / size.width(), tileRect.maxY() / size.height() };
    float scaleX = patternTransform.a(), scaleY = patternTransform.d();
    float tileW = tileRect.width() * scaleX, tileH = tileRect.height() * scaleY;
    float stepX = tileW + spacing.width(), stepY = tileH + spacing.height();
    float originX = phase.x() + tileRect.x() * scaleX + patternTransform.e();
    float originY = phase.y() + tileRect.y() * scaleY + patternTransform.f();
    // one tile across when the pattern does not repeat that way
    bool oneX = !std::isfinite(stepX) || stepX >= std::numeric_limits<float>::max() / 2;
    bool oneY = !std::isfinite(stepY) || stepY >= std::numeric_limits<float>::max() / 2;
    int firstX = oneX ? 0 : static_cast<int>(std::floor((destRect.x() - originX) / stepX));
    int lastX = oneX ? 0 : static_cast<int>(std::ceil((destRect.maxX() - originX) / stepX));
    int firstY = oneY ? 0 : static_cast<int>(std::floor((destRect.y() - originY) / stepY));
    int lastY = oneY ? 0 : static_cast<int>(std::ceil((destRect.maxY() - originY) / stepY));
    if (static_cast<int64_t>(lastX - firstX + 1) * (lastY - firstY + 1) > 16384) {
        trinityNotPorted("patterns over 16384 tiles"_s);
        return;
    }
    int id = image->webgfxImage();
    save();
    clip(destRect);
    for (int j = firstY; j <= lastY; ++j) {
        for (int i = firstX; i <= lastX; ++i) {
            float const dst[4] = { originX + i * stepX, originY + j * stepY, tileW, tileH };
            webgfx_canvas_draw_image(m_canvas, id, dst, uv);
        }
    }
    restore();
}

void GraphicsContextTrinity::drawTrinityGlyphs(int font, float pixelSize, std::span<const uint32_t> glyphs, std::span<const float> xs, std::span<const float> ys)
{
    if (glyphs.empty())
        return;
    if (hasDropShadow())
        trinityNotPorted("text shadows"_s);
    if (fillGradient() || fillPattern())
        trinityNotPorted("gradient and pattern text"_s);
    float rgba[4];
    toRGBA(withAlpha(fillColor()), rgba);
    webgfx_canvas_glyphs(m_canvas, font, pixelSize, glyphs.data(), xs.data(), ys.data(), static_cast<int>(glyphs.size()), rgba);
}

} // namespace WebCore

#endif // USE(TRINITY)
