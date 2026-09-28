// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "CoordinatedPlatformLayerBufferTrinity.h"

#if USE(COORDINATED_GRAPHICS) && USE(TRINITY) && ENABLE(VIDEO)

#include "CoordinatedPlatformLayerBufferYUV.h"
#include "TextureMapper.h"
#include "TextureMapperGLHeaders.h"
#include <atomic>

namespace WebCore {

// three one-channel textures per picture size, on the compositor
struct TrinityPlaneTextures {
    IntSize size;
    std::array<GLuint, 3> textures { };
    uint64_t owner { 0 };
};

static Vector<TrinityPlaneTextures>& planeTextures()
{
    static thread_local Vector<TrinityPlaneTextures> cache;
    return cache;
}

static TrinityPlaneTextures& texturesFor(const IntSize& size)
{
    auto& cache = planeTextures();
    for (auto& entry : cache) {
        if (entry.size == size)
            return entry;
    }
    // a few sizes at most: the oldest goes
    if (cache.size() >= 4) {
        glDeleteTextures(3, cache[0].textures.data());
        cache.removeAt(0);
    }
    TrinityPlaneTextures entry { size, { }, 0 };
    glGenTextures(3, entry.textures.data());
    for (int i = 0; i < 3; ++i) {
        IntSize plane = i ? IntSize(size.width() / 2, size.height() / 2) : size;
        glBindTexture(GL_TEXTURE_2D, entry.textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, plane.width(), plane.height(), 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    cache.append(WTF::move(entry));
    return cache.last();
}

static uint64_t nextBufferID()
{
    static std::atomic<uint64_t> next { 0 };
    return ++next;
}

CoordinatedPlatformLayerBufferTrinity::CoordinatedPlatformLayerBufferTrinity(const IntSize& size, bool bt709, Vector<uint8_t>&& y, Vector<uint8_t>&& u, Vector<uint8_t>&& v)
    : CoordinatedPlatformLayerBuffer(Type::Trinity, size, { }, nullptr)
    , m_id(nextBufferID())
    , m_bt709(bt709)
    , m_y(WTF::move(y))
    , m_u(WTF::move(u))
    , m_v(WTF::move(v))
{
}

void CoordinatedPlatformLayerBufferTrinity::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    if (m_size.isEmpty())
        return;
    auto& planes = texturesFor(m_size);
    // another picture of this size may have used the textures
    if (planes.owner != m_id) {
        std::array<const Vector<uint8_t>*, 3> data { &m_y, &m_u, &m_v };
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        for (int i = 0; i < 3; ++i) {
            IntSize plane = i ? IntSize(m_size.width() / 2, m_size.height() / 2) : m_size;
            glBindTexture(GL_TEXTURE_2D, planes.textures[i]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, plane.width(), plane.height(), GL_LUMINANCE, GL_UNSIGNED_BYTE, data[i]->span().data());
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        planes.owner = m_id;
    }
    auto yuv = CoordinatedPlatformLayerBufferYUV::create(CoordinatedPlatformLayerBufferYUV::Format::YUV420, 3,
        { planes.textures[0], planes.textures[1], planes.textures[2], 0 }, { 0, 1, 2, 0 }, { 0, 0, 0, 0 },
        m_bt709 ? CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Bt709 : CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Bt601,
        CoordinatedPlatformLayerBufferYUV::TransferFunction::Bt709, m_size, m_flags, nullptr);
    static_cast<TextureMapperPlatformLayer&>(*yuv).paintToTextureMapper(textureMapper, targetRect, modelViewMatrix, opacity);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(TRINITY) && ENABLE(VIDEO)
