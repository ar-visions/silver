// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#if ENABLE(MEDIA_SOURCE) && USE(TRINITY)

#include "MediaSourcePrivate.h"
#include <wtf/LoggerHelper.h>

namespace WebCore {

class MediaPlayerPrivateTrinity;
class SourceBufferPrivateTrinity;

class MediaSourcePrivateTrinity final
    : public MediaSourcePrivate
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static Ref<MediaSourcePrivateTrinity> create(MediaPlayerPrivateTrinity&, MediaSourcePrivateClient&);
    virtual ~MediaSourcePrivateTrinity();

    constexpr MediaPlatformType platformType() const final { return MediaPlatformType::Trinity; }
    RefPtr<MediaPlayerPrivateInterface> player() const final;
    void setPlayer(MediaPlayerPrivateInterface*) final;
    RefPtr<MediaPlayerPrivateTrinity> trinityPlayer() const;

    // source buffers waiting for room get their samples
    void provideWaitingSamples();

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    ASCIILiteral logClassName() const final { return "MediaSourcePrivateTrinity"_s; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
    uint64_t nextSourceBufferLogIdentifier() { return childLogIdentifier(m_logIdentifier, ++m_nextSourceBufferID); }
#endif

private:
    MediaSourcePrivateTrinity(MediaPlayerPrivateTrinity&, MediaSourcePrivateClient&);

    AddStatus addSourceBuffer(const ContentType&, const MediaSourceConfiguration&, RefPtr<SourceBufferPrivate>&) final;
    void durationChanged(const MediaTime&) final;
    void notifyActiveSourceBuffersChanged() final;

    WeakPtr<MediaPlayerPrivateTrinity> m_player;
#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
    uint64_t m_nextSourceBufferID { 0 };
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MediaSourcePrivateTrinity)
static bool isType(const WebCore::MediaSourcePrivate& mediaSource) { return mediaSource.platformType() == WebCore::MediaPlatformType::Trinity; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(MEDIA_SOURCE) && USE(TRINITY)
