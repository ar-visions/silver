// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#if USE(TRINITY)

#include "IntSize.h"
#include <span>
#include <wtf/Vector.h>

namespace WebCore {

// premultiplied bgra8 rows (stride bytes apart) as a PNG file
WEBCORE_EXPORT Vector<uint8_t> encodeTrinityPNG(std::span<const uint8_t> bgra, IntSize, unsigned stride);

// straight rgba8 rows as a PNG file
WEBCORE_EXPORT Vector<uint8_t> encodeTrinityPNGFromRGBA(std::span<const uint8_t> rgba, IntSize);

} // namespace WebCore

#endif // USE(TRINITY)
