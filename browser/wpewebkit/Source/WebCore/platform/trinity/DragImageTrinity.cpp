// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "DragImage.h"

#if USE(TRINITY)

#include "Image.h"
#include "NativeImage.h"
#include "WebGfx.h"
#include <wtf/URL.h>

namespace WebCore {

IntSize dragImageSize(DragImageRef image)
{
    return image ? image->size() : IntSize();
}

void deleteDragImage(DragImageRef)
{
}

// drawn at the new size on a trinity canvas, read back
DragImageRef scaleDragImage(DragImageRef image, FloatSize scale)
{
    if (!image)
        return nullptr;
    IntSize size = image->size();
    IntSize scaled(size);
    scaled.scale(scale.width(), scale.height());
    if (scaled == size || scaled.isEmpty())
        return image;
    int canvas = webgfx_canvas_new(scaled.width(), scaled.height());
    if (!canvas)
        return nullptr;
    float const dst[4] = { 0, 0, static_cast<float>(scaled.width()), static_cast<float>(scaled.height()) };
    float const uv[4] = { 0, 0, 1, 1 };
    webgfx_canvas_draw_image(canvas, image->webgfxImage(), dst, uv);
    Vector<uint8_t> rgba(scaled.area() * 4);
    webgfx_canvas_read(canvas, rgba.mutableSpan().data());
    webgfx_canvas_free(canvas);
    // the canvas is premultiplied; images are straight
    for (size_t i = 0; i < rgba.size(); i += 4) {
        uint8_t a = rgba[i + 3];
        for (int c = 0; c < 3 && a && a != 255; ++c)
            rgba[i + c] = std::min(255, rgba[i + c] * 255 / a);
    }
    return TrinityImage::create(scaled, WTF::move(rgba));
}

DragImageRef dissolveDragImageToFraction(DragImageRef image, float fraction)
{
    if (!image)
        return nullptr;
    auto pixels = image->pixels();
    Vector<uint8_t> rgba(pixels.size());
    for (size_t i = 0; i < pixels.size(); i += 4) {
        rgba[i] = pixels[i];
        rgba[i + 1] = pixels[i + 1];
        rgba[i + 2] = pixels[i + 2];
        rgba[i + 3] = static_cast<uint8_t>(pixels[i + 3] * fraction);
    }
    return TrinityImage::create(image->size(), WTF::move(rgba));
}

DragImageRef createDragImageFromImage(Image* image, ImageOrientation, GraphicsClient*, float)
{
    auto nativeImage = image->currentNativeImage();
    return nativeImage ? nativeImage->platformImage() : nullptr;
}

DragImageRef createDragImageIconForCachedImageFilename(const String&)
{
    return nullptr;
}

DragImageData createDragImageForLink(Element&, URL&, const String&, float)
{
    return { nullptr, nullptr };
}

DragImageRef createDragImageForColor(const Color&, const FloatRect&, float, Path&)
{
    return nullptr;
}

} // namespace WebCore

#endif // USE(TRINITY)
