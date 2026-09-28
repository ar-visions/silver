// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "NativeImage.h"

#if USE(TRINITY)

#include "TrinityImage.h"

namespace WebCore {

IntSize NativeImage::size() const
{
    return m_platformImage ? m_platformImage->size() : IntSize();
}

bool NativeImage::hasAlpha() const
{
    return m_platformImage && m_platformImage->hasAlpha();
}

DestinationColorSpace NativeImage::colorSpace() const
{
    return DestinationColorSpace::SRGB();
}

std::optional<Color> NativeImage::singlePixelSolidColor() const
{
    if (size() != IntSize(1, 1))
        return std::nullopt;
    auto pixel = m_platformImage->pixels();
    return Color(SRGBA<uint8_t> { pixel[0], pixel[1], pixel[2], pixel[3] });
}

void NativeImage::clearSubimages()
{
}

uint64_t NativeImage::uniqueID() const
{
    return reinterpret_cast<uintptr_t>(m_platformImage.get());
}

} // namespace WebCore

#endif // USE(TRINITY)
