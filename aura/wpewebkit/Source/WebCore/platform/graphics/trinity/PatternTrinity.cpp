// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "Pattern.h"

#if USE(TRINITY)

#include "NativeImage.h"

namespace WebCore {

PlatformPatternPtr Pattern::createPlatformPattern(const AffineTransform& userSpaceTransform) const
{
    auto image = tileNativeImage();
    if (!image || !image->platformImage())
        return nullptr;
    auto pattern = adoptRef(*new TrinityPattern);
    pattern->tile = image->platformImage();
    pattern->transform = userSpaceTransform * patternSpaceTransform();
    pattern->repeatX = repeatX();
    pattern->repeatY = repeatY();
    return pattern;
}

} // namespace WebCore

#endif // USE(TRINITY)
