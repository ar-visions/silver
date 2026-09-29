// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#if USE(TRINITY)

#include "GraphicsContext.h"
#include "IntRect.h"
#include <wtf/Vector.h>

namespace WebCore {

// logs a missing feature once, so gaps show and never pass
WEBCORE_EXPORT void trinityNotPorted(ASCIILiteral);

// WebKit's painting onto a webgfx canvas (0: no canvas)
class WEBCORE_EXPORT GraphicsContextTrinity final : public GraphicsContext {
public:
    GraphicsContextTrinity(int canvas, RenderingMode, RenderingPurpose, IntSize = { });
    virtual ~GraphicsContextTrinity();

    int canvas() const { return m_canvas; }
    // only a trinity context answers: filters find its canvas
    PlatformGraphicsContext* platformContext() const final { return const_cast<GraphicsContextTrinity*>(this); }

    void didUpdateState(GraphicsContextState&) final;

    void setLineCap(LineCap) final;
    void setLineDash(const DashArray&, float) final;
    void setLineJoin(LineJoin) final;
    void setMiterLimit(float) final;
    LineCap lineCap() const { return m_stroke.cap; }
    LineJoin lineJoin() const { return m_stroke.join; }
    float miterLimit() const { return m_stroke.miter; }

    using GraphicsContext::fillRect;
    void fillRect(const FloatRect&, RequiresClipToRect = RequiresClipToRect::Yes) final;
    void fillRect(const FloatRect&, const Color&) final;
    void fillRect(const FloatRect&, Gradient&, const AffineTransform&, RequiresClipToRect = RequiresClipToRect::Yes) final;
    void fillRoundedRectImpl(const FloatRoundedRect&, const Color&) final;
    void fillPath(const Path&) final;
    void strokeRect(const FloatRect&, float) final;
    void strokePath(const Path&) final;
    void clearRect(const FloatRect&) final;

    void drawNativeImage(const NativeImage&, const FloatRect&, const FloatRect&, ImagePaintingOptions) final;
    void drawImageBuffer(ImageBuffer&, const FloatRect&, const FloatRect&, ImagePaintingOptions) final;
    void drawConsumingImageBuffer(RefPtr<ImageBuffer>, const FloatRect&, const FloatRect&, ImagePaintingOptions) final;
#if ENABLE(VIDEO)
    void drawVideoFrame(const VideoFrame&, const FloatRect&, ImageOrientation, bool shouldDiscardAlpha) final;
#endif
    void drawPattern(const NativeImage&, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions) final;
    void drawRect(const FloatRect&, float) final;
    void drawLine(const FloatPoint&, const FloatPoint&) final;
    void drawLinesForText(const FloatPoint&, float thickness, std::span<const FloatSegment>, bool isPrinting, bool doubleLines, StrokeStyle) final;
    void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) final;
    void drawEllipse(const FloatRect&) final;

    void drawFocusRing(const Path&, float outlineWidth, const Color&, float zoomFactor) final;
    void drawFocusRing(const Vector<FloatRect>&, float outlineWidth, const Color&, float zoomFactor) final;

    void save(GraphicsContextState::Purpose = GraphicsContextState::Purpose::SaveRestore) final;
    void restore(GraphicsContextState::Purpose = GraphicsContextState::Purpose::SaveRestore) final;

    void translate(float, float) final;
    void rotate(float) final;
    using GraphicsContext::scale;
    void scale(const FloatSize&) final;
    void concatCTM(const AffineTransform&) final;
    void setCTM(const AffineTransform&) final;
    AffineTransform getCTM(GraphicsContext::IncludeDeviceScale) const final;

    void beginTransparencyLayer(float) final;
    void beginTransparencyLayer(CompositeOperator, BlendMode) final;
    void endTransparencyLayer() final;

    void resetClip() final;
    void clip(const FloatRect&) final;
    void clipOut(const FloatRect&) final;
    void clipOut(const Path&) final;
    void clipPath(const Path&, WindRule) final;
    void clipRoundedRect(const FloatRoundedRect&) final;
    IntRect clipBounds() const final;
    void clipToImageBuffer(ImageBuffer&, const FloatRect&) final;

    RenderingMode renderingMode() const final { return m_renderingMode; }

    // premultiplied bgra8 memory the canvas mirrors; owns it
    void attachBacking(std::span<uint8_t>, unsigned bytesPerRow, Function<void()>&& release);
    void flushToBacking();
    void loadFromBacking();

    // glyph numbers at baseline pens, in user space
    void drawTrinityGlyphs(int font, float pixelSize, std::span<const uint32_t> glyphs, std::span<const float> xs, std::span<const float> ys);

private:
    void applyTransform();
    void applyTransformTo(int canvas);
    void clipDevice(const IntRect&);
    void fillColorRect(const FloatRect&, const Color&);
    void setShadowForDraw();
    void fillWithBrush(const Function<void(const Color&)>&);
    bool setGradient(const Gradient&, const AffineTransform& gradientSpaceTransform);
    void clearGradient();
    void pushLayer(float opacity, CompositeOperator);
    void popLayer();
    Color withAlpha(const Color&) const;

    struct Stroke {
        LineCap cap { LineCap::Butt };
        LineJoin join { LineJoin::Miter };
        float miter { 4 };
    };
    // an open transparency layer and the canvas under it
    struct Layer {
        int canvas { 0 };
        int parent { 0 };
        float opacity { 1 };
        CompositeOperator operation { CompositeOperator::SourceOver };
    };
    struct Frame {
        AffineTransform ctm;
        IntRect clip;
        Stroke stroke;
    };

    int m_canvas { 0 };
    IntSize m_size;
    RenderingMode m_renderingMode { RenderingMode::Unaccelerated };
    RenderingPurpose m_renderingPurpose { RenderingPurpose::Unspecified };
    AffineTransform m_ctm;
    IntRect m_clip;
    Stroke m_stroke;
    Vector<Frame> m_frames;
    Vector<Layer> m_layers;
    std::span<uint8_t> m_backing;
    unsigned m_backingBytesPerRow { 0 };
    Function<void()> m_releaseBacking;
};

} // namespace WebCore

#endif // USE(TRINITY)
