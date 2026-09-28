// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "TrinityPNG.h"

#if USE(TRINITY)

#include <png.h>

namespace WebCore {

Vector<uint8_t> encodeTrinityPNGFromRGBA(std::span<const uint8_t> rgba, IntSize size)
{
    if (size.isEmpty() || rgba.size() < static_cast<size_t>(size.area()) * 4)
        return { };
    png_image image { };
    image.version = PNG_IMAGE_VERSION;
    image.width = size.width();
    image.height = size.height();
    image.format = PNG_FORMAT_RGBA;
    png_alloc_size_t bytes = 0;
    if (!png_image_write_to_memory(&image, nullptr, &bytes, 0, rgba.data(), 0, nullptr) || !bytes)
        return { };
    Vector<uint8_t> out(bytes);
    if (!png_image_write_to_memory(&image, out.mutableSpan().data(), &bytes, 0, rgba.data(), 0, nullptr))
        return { };
    out.shrink(bytes);
    return out;
}

Vector<uint8_t> encodeTrinityPNG(std::span<const uint8_t> bgra, IntSize size, unsigned stride)
{
    if (size.isEmpty() || bgra.size() < static_cast<size_t>(stride) * (size.height() - 1) + size.width() * 4)
        return { };
    Vector<uint8_t> rgba(size.area() * 4);
    for (int y = 0; y < size.height(); ++y) {
        auto* from = bgra.data() + y * stride;
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
    return encodeTrinityPNGFromRGBA(rgba.span(), size);
}

} // namespace WebCore

#endif // USE(TRINITY)
