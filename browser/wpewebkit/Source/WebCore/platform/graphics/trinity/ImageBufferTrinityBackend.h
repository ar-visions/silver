// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#if USE(TRINITY)

#include "GraphicsContextTrinity.h"
#include "ImageBuffer.h"
#include "ImageBufferBackend.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

// an offscreen trinity canvas; pixels read back on demand
class ImageBufferTrinityBackend final : public ImageBufferBackend {
    WTF_MAKE_TZONE_ALLOCATED(ImageBufferTrinityBackend);
    WTF_MAKE_NONCOPYABLE(ImageBufferTrinityBackend);
public:
    static IntSize calculateSafeBackendSize(const Parameters&);
    static size_t calculateMemoryCost(const Parameters&);
    WEBCORE_EXPORT static std::unique_ptr<ImageBufferTrinityBackend> create(const Parameters&, const ImageBufferCreationContext&);
    WEBCORE_EXPORT ~ImageBufferTrinityBackend();

    int canvas() const { return m_canvas; }

private:
    ImageBufferTrinityBackend(const Parameters&, int canvas);

    GraphicsContext& context() LIFETIME_BOUND final { return m_context; }
    unsigned bytesPerRow() const final;
    bool canMapBackingStore() const final { return false; }
    String debugDescription() const final;

    RefPtr<NativeImage> copyNativeImage() final;
    RefPtr<NativeImage> createNativeImageReference() final;
    void getPixelBuffer(const IntRect&, PixelBuffer&) final;
    void putPixelBuffer(const PixelBufferSourceView&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat) final;

    // premultiplied bgra8 of the canvas as drawn so far
    std::span<uint8_t> readBack();

    int m_canvas { 0 };
    GraphicsContextTrinity m_context;
    Vector<uint8_t> m_pixels;
};

} // namespace WebCore

#endif // USE(TRINITY)
