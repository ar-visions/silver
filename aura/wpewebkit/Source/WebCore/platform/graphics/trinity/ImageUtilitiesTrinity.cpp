// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "ImageUtilities.h"

#if USE(TRINITY)

#include "GraphicsContextTrinity.h"
#include "NativeImage.h"
#include "TrinityImage.h"
#include "TrinityPNG.h"

namespace WebCore {

Vector<uint8_t> platformEncodeData(const NativeImage& nativeImage, const String& mimeType, std::optional<double>)
{
    auto& image = nativeImage.platformImage();
    if (!image)
        return { };
    if (mimeType != "image/png"_s) {
        trinityNotPorted("image encoding other than PNG"_s);
        return { };
    }
    return encodeTrinityPNGFromRGBA(image->pixels(), image->size());
}

} // namespace WebCore

#endif // USE(TRINITY)
