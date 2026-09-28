// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#if ENABLE(VIDEO) && USE(TRINITY)

#include "VideoFrame.h"

namespace WebCore {

// the frame a webgfx video shows now; drawn from its gpu planes
class VideoFrameTrinity final : public VideoFrame {
public:
    static Ref<VideoFrameTrinity> create(int video, IntSize size, MediaTime time)
    {
        return adoptRef(*new VideoFrameTrinity(video, size, time));
    }

    int video() const { return m_video; }
    IntSize presentationSize() const final { return m_size; }
    // planar 4:2:0, 'y420'
    uint32_t pixelFormat() const final { return 0x79343230; }
    bool isTrinity() const final { return true; }

private:
    VideoFrameTrinity(int video, IntSize size, MediaTime time)
        : VideoFrame(time, false, Rotation::None)
        , m_video(video)
        , m_size(size)
    {
    }

    Ref<VideoFrame> clone() final { return create(m_video, m_size, presentationTime()); }

    int m_video { 0 };
    IntSize m_size;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::VideoFrameTrinity)
static bool isType(const WebCore::VideoFrame& frame) { return frame.isTrinity(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(VIDEO) && USE(TRINITY)
