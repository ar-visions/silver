// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "ImageBackingStore.h"

#if USE(TRINITY)

#include "TrinityImage.h"

namespace WebCore {

// decoded bgra8 (premultiplied or not) to straight rgba8
PlatformImagePtr ImageBackingStore::image() const
{
    auto size = this->size();
    Vector<uint8_t> rgba(size.area() * 4);
    bool hasAlpha = false;
    for (size_t i = 0; i < m_pixelsSpan.size() && i < static_cast<size_t>(size.area()); ++i) {
        uint32_t pixel = m_pixelsSpan[i];
        uint8_t a = pixel >> 24, r = pixel >> 16, g = pixel >> 8, b = pixel;
        if (m_premultiplyAlpha && a && a != 255) {
            r = std::min(255, r * 255 / a);
            g = std::min(255, g * 255 / a);
            b = std::min(255, b * 255 / a);
        }
        hasAlpha |= a != 255;
        rgba[i * 4 + 0] = r;
        rgba[i * 4 + 1] = g;
        rgba[i * 4 + 2] = b;
        rgba[i * 4 + 3] = a;
    }
    return TrinityImage::create(size, WTF::move(rgba), hasAlpha);
}

} // namespace WebCore

#endif // USE(TRINITY)
