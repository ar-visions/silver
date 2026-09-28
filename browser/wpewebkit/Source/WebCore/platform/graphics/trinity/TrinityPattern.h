// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#if USE(TRINITY)

#include "AffineTransform.h"
#include "TrinityImage.h"
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

// an image repeated across its pattern space
struct TrinityPattern : public ThreadSafeRefCounted<TrinityPattern> {
    RefPtr<TrinityImage> tile;
    AffineTransform transform;
    bool repeatX { true };
    bool repeatY { true };
};

} // namespace WebCore

#endif // USE(TRINITY)
