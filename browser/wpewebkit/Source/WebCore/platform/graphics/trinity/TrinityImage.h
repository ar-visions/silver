// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#if USE(TRINITY)

#include "IntSize.h"
#include <span>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

// straight-alpha rgba8 pixels; the GPU copy is made on first draw
class TrinityImage : public ThreadSafeRefCounted<TrinityImage> {
public:
    WEBCORE_EXPORT static Ref<TrinityImage> create(IntSize, Vector<uint8_t>&& rgba, bool hasAlpha = true);
    WEBCORE_EXPORT ~TrinityImage();

    IntSize size() const { return m_size; }
    bool hasAlpha() const { return m_hasAlpha; }
    std::span<const uint8_t> pixels() const LIFETIME_BOUND { return m_pixels.span(); }

    // premultiplied bgra8 rows, width * 4 bytes apart
    WEBCORE_EXPORT Vector<uint8_t> bgraPremultiplied() const;

    // the webgfx image id, uploaded once
    WEBCORE_EXPORT int webgfxImage() const;

private:
    TrinityImage(IntSize, Vector<uint8_t>&&, bool hasAlpha);

    IntSize m_size;
    Vector<uint8_t> m_pixels;
    bool m_hasAlpha { true };
    mutable Lock m_lock;
    mutable int m_image WTF_GUARDED_BY_LOCK(m_lock) { 0 };
};

} // namespace WebCore

#endif // USE(TRINITY)
