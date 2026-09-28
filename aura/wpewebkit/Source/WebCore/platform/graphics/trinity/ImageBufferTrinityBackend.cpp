// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "ImageBufferTrinityBackend.h"

#if USE(TRINITY)

#include "IntRect.h"
#include "NativeImage.h"
#include "PixelBuffer.h"
#include "TrinityImage.h"
#include "WebGfx.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ImageBufferTrinityBackend);

IntSize ImageBufferTrinityBackend::calculateSafeBackendSize(const Parameters& parameters)
{
    IntSize size = parameters.backendSize;
    if (size.isEmpty())
        return size;
    CheckedSize bytes = CheckedUint32(size.height()) * (CheckedUint32(size.width()) * 4);
    if (bytes.hasOverflowed())
        return { };
    return size;
}

size_t ImageBufferTrinityBackend::calculateMemoryCost(const Parameters& parameters)
{
    return ImageBufferBackend::calculateMemoryCost(parameters.backendSize, parameters.backendSize.width() * 4);
}

std::unique_ptr<ImageBufferTrinityBackend> ImageBufferTrinityBackend::create(const Parameters& parameters, const ImageBufferCreationContext&)
{
    auto size = calculateSafeBackendSize(parameters);
    if (size.isEmpty())
        return nullptr;
    auto format = parameters.bufferFormat.pixelFormat;
    if (format != PixelFormat::BGRA8 && format != PixelFormat::BGRX8 && format != PixelFormat::RGBA8)
        return nullptr;
    int canvas = webgfx_canvas_new(size.width(), size.height());
    if (!canvas)
        return nullptr;
    return std::unique_ptr<ImageBufferTrinityBackend>(new ImageBufferTrinityBackend(parameters, canvas));
}

ImageBufferTrinityBackend::ImageBufferTrinityBackend(const Parameters& parameters, int canvas)
    : ImageBufferBackend(parameters)
    , m_canvas(canvas)
    , m_context(canvas, parameters.purpose == RenderingPurpose::Canvas ? RenderingMode::Accelerated : RenderingMode::Unaccelerated, parameters.purpose, parameters.backendSize)
{
    m_context.applyDeviceScaleFactor(parameters.resolutionScale);
}

ImageBufferTrinityBackend::~ImageBufferTrinityBackend()
{
    webgfx_canvas_free(m_canvas);
}

unsigned ImageBufferTrinityBackend::bytesPerRow() const
{
    return size().width() * 4;
}

String ImageBufferTrinityBackend::debugDescription() const
{
    TextStream stream;
    stream << "ImageBufferTrinityBackend " << this;
    return stream.release();
}

// the canvas holds premultiplied rgba8
std::span<uint8_t> ImageBufferTrinityBackend::readBack()
{
    size_t bytes = size().area() * 4;
    m_pixels.resize(bytes);
    webgfx_canvas_read(m_canvas, m_pixels.mutableSpan().data());
    if (pixelFormat() != PixelFormat::RGBA8) {
        for (size_t i = 0; i < bytes; i += 4)
            std::swap(m_pixels[i], m_pixels[i + 2]);
    }
    return m_pixels.mutableSpan();
}

RefPtr<NativeImage> ImageBufferTrinityBackend::copyNativeImage()
{
    auto pixels = readBack();
    bool red = pixelFormat() == PixelFormat::RGBA8;
    // images are straight rgba8
    Vector<uint8_t> rgba(pixels.size());
    for (size_t i = 0; i < pixels.size(); i += 4) {
        uint8_t a = pixels[i + 3];
        uint8_t r = pixels[i + (red ? 0 : 2)];
        uint8_t g = pixels[i + 1];
        uint8_t b = pixels[i + (red ? 2 : 0)];
        if (a && a != 255) {
            r = std::min(255, r * 255 / a);
            g = std::min(255, g * 255 / a);
            b = std::min(255, b * 255 / a);
        }
        rgba[i] = r;
        rgba[i + 1] = g;
        rgba[i + 2] = b;
        rgba[i + 3] = a;
    }
    return NativeImage::create(TrinityImage::create(size(), WTF::move(rgba), pixelFormat() != PixelFormat::BGRX8));
}

RefPtr<NativeImage> ImageBufferTrinityBackend::createNativeImageReference()
{
    return copyNativeImage();
}

void ImageBufferTrinityBackend::getPixelBuffer(const IntRect& srcRect, PixelBuffer& destination)
{
    ImageBufferBackend::getPixelBuffer(srcRect, readBack(), destination);
}

// written into the read back pixels, then drawn back whole
void ImageBufferTrinityBackend::putPixelBuffer(const PixelBufferSourceView& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    auto pixels = readBack();
    ImageBufferBackend::putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat, pixels);
    auto image = copyNativeImage();
    if (!image)
        return;
    auto size = this->size();
    float const clear[4] = { 0, 0, 0, 0 };
    float const identity[6] = { 1, 0, 0, 1, 0, 0 };
    float const dst[4] = { 0, 0, static_cast<float>(size.width()), static_cast<float>(size.height()) };
    float const uv[4] = { 0, 0, 1, 1 };
    webgfx_canvas_save(m_canvas);
    webgfx_canvas_transform(m_canvas, identity);
    webgfx_canvas_clear(m_canvas, clear);
    webgfx_canvas_draw_image(m_canvas, image->platformImage()->webgfxImage(), dst, uv);
    webgfx_canvas_restore(m_canvas);
}

} // namespace WebCore

#endif // USE(TRINITY)
