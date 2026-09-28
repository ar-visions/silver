// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "ShareableBitmap.h"

#if USE(TRINITY)

#include "BitmapImage.h"
#include "GraphicsContextTrinity.h"
#include "NativeImage.h"
#include "TrinityImage.h"
#include "WebGfx.h"

namespace WebCore {

std::optional<DestinationColorSpace> ShareableBitmapConfiguration::validateColorSpace(std::optional<DestinationColorSpace> colorSpace)
{
    return colorSpace;
}

// premultiplied bgra8, four bytes a pixel
CheckedUint32 ShareableBitmapConfiguration::calculateBitsPerComponent(const DestinationColorSpace&)
{
    return 8;
}

CheckedUint32 ShareableBitmapConfiguration::calculateBytesPerPixel(const DestinationColorSpace&)
{
    return 4;
}

CheckedUint32 ShareableBitmapConfiguration::calculateBytesPerRow(const IntSize& size, const DestinationColorSpace&)
{
    return CheckedUint32(size.width()) * 4;
}

std::unique_ptr<GraphicsContext> ShareableBitmap::createGraphicsContext()
{
    auto size = this->size();
    int canvas = webgfx_canvas_new(size.width(), size.height());
    if (!canvas)
        return nullptr;
    auto context = makeUnique<GraphicsContextTrinity>(canvas, RenderingMode::Unaccelerated, RenderingPurpose::ShareableSnapshot, size);
    ref();
    context->attachBacking(mutableSpan(), bytesPerRow(), [this] {
        deref();
    });
    return context;
}

void ShareableBitmap::paint(GraphicsContext& context, const IntPoint& dstPoint, const IntRect& srcRect)
{
    paint(context, 1, dstPoint, srcRect);
}

void ShareableBitmap::paint(GraphicsContext& context, float scaleFactor, const IntPoint& dstPoint, const IntRect& srcRect)
{
    FloatRect scaledSrcRect(srcRect);
    scaledSrcRect.scale(scaleFactor);
    FloatRect scaledDestRect(dstPoint, srcRect.size());
    scaledDestRect.scale(scaleFactor);
    if (context.compositeMode().operation == CompositeOperator::Copy)
        trinityNotPorted("copy blend of shareable bitmaps"_s);
    if (RefPtr image = NativeImage::create(createPlatformImage(BackingStoreCopy::DontCopyBackingStore)))
        context.drawNativeImage(*image, scaledDestRect, scaledSrcRect, { });
}

RefPtr<Image> ShareableBitmap::createImage()
{
    return BitmapImage::create(createPlatformImage(BackingStoreCopy::DontCopyBackingStore));
}

// images are straight rgba8: always a converted copy
PlatformImagePtr ShareableBitmap::createBasePlatformImage(BackingStoreCopy, ShouldInterpolate)
{
    auto size = this->size();
    auto pixels = span();
    unsigned stride = bytesPerRow();
    Vector<uint8_t> rgba(size.area() * 4);
    for (int y = 0; y < size.height(); ++y) {
        auto* from = pixels.data() + y * stride;
        auto* to = rgba.mutableSpan().data() + y * size.width() * 4;
        for (int x = 0; x < size.width(); ++x) {
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
    return TrinityImage::create(size, WTF::move(rgba));
}

PlatformImagePtr ShareableBitmap::createPlatformImage(BackingStoreCopy copyBehavior, ShouldInterpolate shouldInterpolate)
{
    return createBasePlatformImage(copyBehavior, shouldInterpolate);
}

void ShareableBitmap::setOwnershipOfMemory(const ProcessIdentity&)
{
}

} // namespace WebCore

#endif // USE(TRINITY)
