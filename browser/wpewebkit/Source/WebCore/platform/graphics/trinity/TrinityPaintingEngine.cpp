// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "TrinityPaintingEngine.h"

#if USE(TRINITY)

#include "CoordinatedTileBuffer.h"
#include "GraphicsContextTrinity.h"
#include "GraphicsLayer.h"
#include "WebGfx.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TrinityPaintingEngine);

std::unique_ptr<TrinityPaintingEngine> TrinityPaintingEngine::create()
{
    return std::unique_ptr<TrinityPaintingEngine>(new TrinityPaintingEngine);
}

TrinityPaintingEngine::~TrinityPaintingEngine()
{
    if (m_canvas)
        webgfx_canvas_free(m_canvas);
}

// grows only: tiles vary in size, the canvas is reused
void TrinityPaintingEngine::ensureCanvas(IntSize size)
{
    if (m_canvas && m_canvasSize.width() >= size.width() && m_canvasSize.height() >= size.height())
        return;
    if (m_canvas)
        webgfx_canvas_free(m_canvas);
    m_canvasSize = m_canvasSize.expandedTo(size);
    m_canvas = webgfx_canvas_new(m_canvasSize.width(), m_canvasSize.height());
    m_pixels.resize(m_canvasSize.area() * 4);
}

void TrinityPaintingEngine::paint(GraphicsLayer& layer, CoordinatedTileBuffer& buffer, const IntRect& sourceRect, const IntRect& mappedSourceRect, const IntRect& targetRect, float contentsScale)
{
    auto& tile = static_cast<CoordinatedUnacceleratedTileBuffer&>(buffer);
    auto size = buffer.size();
    if (size.isEmpty())
        return;
    buffer.beginPainting();
    ensureCanvas(size);

    float const clear[4] = { 0, 0, 0, 0 };
    webgfx_canvas_clear(m_canvas, clear);
    {
        GraphicsContextTrinity context(m_canvas, RenderingMode::Accelerated, RenderingPurpose::LayerBacking, size);
        context.clip(targetRect);
        context.translate(targetRect.x(), targetRect.y());
        context.translate(-sourceRect.x(), -sourceRect.y());
        context.scale(FloatSize(contentsScale, contentsScale));
        layer.paintGraphicsLayerContents(context, mappedSourceRect);
    }

    // rgba8 rows of the canvas into the tile's bgra8 rows
    webgfx_canvas_read_region(m_canvas, 0, 0, size.width(), size.height(), m_pixels.mutableSpan().data());
    auto* out = tile.data();
    int canvasStride = size.width() * 4;
    for (int y = 0; y < size.height(); ++y) {
        auto* from = m_pixels.span().data() + y * canvasStride;
        auto* to = out + y * tile.stride();
        for (int x = 0; x < size.width(); ++x) {
            to[x * 4 + 0] = from[x * 4 + 2];
            to[x * 4 + 1] = from[x * 4 + 1];
            to[x * 4 + 2] = from[x * 4 + 0];
            to[x * 4 + 3] = from[x * 4 + 3];
        }
    }
    buffer.completePainting();
    // this thread's freed silver objects go now
    webgfx_drain();
}

} // namespace WebCore

#endif // USE(TRINITY)
