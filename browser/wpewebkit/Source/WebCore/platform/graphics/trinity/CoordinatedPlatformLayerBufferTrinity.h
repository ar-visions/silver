// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#if USE(COORDINATED_GRAPHICS) && USE(TRINITY) && ENABLE(VIDEO)

#include "CoordinatedPlatformLayerBuffer.h"
#include <wtf/Vector.h>

namespace WebCore {

// one decoded picture's y, u, v planes, drawn by the compositor
class CoordinatedPlatformLayerBufferTrinity final : public CoordinatedPlatformLayerBuffer {
public:
    static std::unique_ptr<CoordinatedPlatformLayerBufferTrinity> create(const IntSize& size, bool bt709, Vector<uint8_t>&& y, Vector<uint8_t>&& u, Vector<uint8_t>&& v)
    {
        return std::unique_ptr<CoordinatedPlatformLayerBufferTrinity>(new CoordinatedPlatformLayerBufferTrinity(size, bt709, WTF::move(y), WTF::move(u), WTF::move(v)));
    }

private:
    CoordinatedPlatformLayerBufferTrinity(const IntSize&, bool bt709, Vector<uint8_t>&&, Vector<uint8_t>&&, Vector<uint8_t>&&);

    void paintToTextureMapper(TextureMapper&, const FloatRect&, const TransformationMatrix& modelViewMatrix = TransformationMatrix(), float opacity = 1.0) final;

    uint64_t m_id;
    bool m_bt709;
    Vector<uint8_t> m_y;
    Vector<uint8_t> m_u;
    Vector<uint8_t> m_v;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(TRINITY) && ENABLE(VIDEO)
