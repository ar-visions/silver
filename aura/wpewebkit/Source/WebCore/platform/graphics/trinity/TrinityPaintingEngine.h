// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#if USE(TRINITY)

#include "IntRect.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

class CoordinatedTileBuffer;
class GraphicsLayer;

// paints a layer's dirty tiles on one reused trinity canvas
class TrinityPaintingEngine {
    WTF_MAKE_TZONE_ALLOCATED(TrinityPaintingEngine);
public:
    WEBCORE_EXPORT static std::unique_ptr<TrinityPaintingEngine> create();
    WEBCORE_EXPORT ~TrinityPaintingEngine();

    WEBCORE_EXPORT void paint(GraphicsLayer&, CoordinatedTileBuffer&, const IntRect& sourceRect, const IntRect& mappedSourceRect, const IntRect& targetRect, float contentsScale);

private:
    TrinityPaintingEngine() = default;
    void ensureCanvas(IntSize);

    int m_canvas { 0 };
    IntSize m_canvasSize;
    Vector<uint8_t> m_pixels;
};

} // namespace WebCore

#endif // USE(TRINITY)
