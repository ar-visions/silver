// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "GraphicsContextGL.h"

#if ENABLE(WEBGL) && USE(TRINITY)

#include "BitmapImage.h"
#include "GraphicsContextGLImageExtractor.h"
#include "NativeImage.h"
#include "PixelBuffer.h"
#include "TrinityImage.h"

namespace WebCore {

GraphicsContextGLImageExtractor::~GraphicsContextGLImageExtractor() = default;

// trinity images are straight rgba8, tightly packed
bool GraphicsContextGLImageExtractor::extractImage(bool premultiplyAlpha, bool ignoreGammaAndColorProfile, bool)
{
    RefPtr<NativeImage> nativeImage;
    bool hasAlpha = !m_image->currentFrameKnownToBeOpaque();
    if ((ignoreGammaAndColorProfile || (hasAlpha && !premultiplyAlpha)) && m_image->data()) {
        auto image = BitmapImage::create(nullptr, AlphaOption::NotPremultiplied, ignoreGammaAndColorProfile ? GammaAndColorProfileOption::Ignored : GammaAndColorProfileOption::Applied);
        image->setData(m_image->data(), true);
        if (!image->frameCount())
            return false;
        nativeImage = image->currentNativeImage();
    } else
        nativeImage = m_image->currentNativeImage();
    if (!nativeImage || !nativeImage->platformImage())
        return false;
    auto& platformImage = nativeImage->platformImage();
    m_imageWidth = platformImage->size().width();
    m_imageHeight = platformImage->size().height();
    if (!m_imageWidth || !m_imageHeight)
        return false;
    m_alphaOp = premultiplyAlpha && platformImage->hasAlpha() ? AlphaOp::DoPremultiply : AlphaOp::DoNothing;
    m_pixelData = Vector<uint8_t> { platformImage->pixels() };
    m_imagePixelData = m_pixelData.span();
    m_imageSourceFormat = DataFormat::RGBA8;
    m_imageSourceUnpackAlignment = 1;
    return true;
}

RefPtr<NativeImage> GraphicsContextGL::createNativeImageFromPixelBuffer(const GraphicsContextGLAttributes& attributes, Ref<PixelBuffer>&& pixelBuffer)
{
    auto size = pixelBuffer->size();
    auto bytes = pixelBuffer->bytes();
    Vector<uint8_t> rgba(bytes);
    bool premultiplied = attributes.alpha && attributes.premultipliedAlpha;
    for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
        if (!attributes.alpha)
            rgba[i + 3] = 255;
        uint8_t a = rgba[i + 3];
        for (int c = 0; c < 3 && premultiplied && a && a != 255; ++c)
            rgba[i + c] = std::min(255, rgba[i + c] * 255 / a);
    }
    return NativeImage::create(TrinityImage::create(size, WTF::move(rgba), attributes.alpha));
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(TRINITY)
