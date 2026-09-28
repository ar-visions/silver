// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "TrinityImage.h"

#if USE(TRINITY)

#include "WebGfx.h"

namespace WebCore {

Ref<TrinityImage> TrinityImage::create(IntSize size, Vector<uint8_t>&& rgba, bool hasAlpha)
{
    return adoptRef(*new TrinityImage(size, WTF::move(rgba), hasAlpha));
}

TrinityImage::TrinityImage(IntSize size, Vector<uint8_t>&& rgba, bool hasAlpha)
    : m_size(size)
    , m_pixels(WTF::move(rgba))
    , m_hasAlpha(hasAlpha)
{
}

TrinityImage::~TrinityImage()
{
    Locker locker { m_lock };
    if (m_image)
        webgfx_image_free(m_image);
}

Vector<uint8_t> TrinityImage::bgraPremultiplied() const
{
    Vector<uint8_t> bgra(m_pixels.size());
    for (size_t i = 0; i + 3 < m_pixels.size(); i += 4) {
        uint8_t a = m_pixels[i + 3];
        bgra[i + 0] = m_pixels[i + 2] * a / 255;
        bgra[i + 1] = m_pixels[i + 1] * a / 255;
        bgra[i + 2] = m_pixels[i + 0] * a / 255;
        bgra[i + 3] = a;
    }
    return bgra;
}

int TrinityImage::webgfxImage() const
{
    Locker locker { m_lock };
    if (!m_image && !m_size.isEmpty())
        m_image = webgfx_image_new(m_pixels.span().data(), m_size.width(), m_size.height());
    return m_image;
}

} // namespace WebCore

#endif // USE(TRINITY)
