// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#if ENABLE(MEDIA_SOURCE) && USE(TRINITY)

#include "SourceBufferPrivate.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WebCore {

class MediaSourcePrivateTrinity;
class TrinityTrackConfig;

// appended mp4 becomes tracks and samples (webgfx demux)
class SourceBufferPrivateTrinity final : public SourceBufferPrivate {
public:
    static Ref<SourceBufferPrivateTrinity> create(MediaSourcePrivateTrinity&);
    virtual ~SourceBufferPrivateTrinity();

    constexpr MediaPlatformType platformType() const final { return MediaPlatformType::Trinity; }

    // tracks that asked to be told when there is room again
    void provideWaitingSamples();

private:
    explicit SourceBufferPrivateTrinity(MediaSourcePrivateTrinity&);
    RefPtr<MediaSourcePrivateTrinity> mediaSourcePrivate() const;

    Ref<MediaPromise> appendInternal(Ref<SharedBuffer>&&) final;
    void resetParserStateInternal() final;
    bool canSwitchToType(const ContentType&) final;
    void flush(TrackID) final;
    void enqueueSample(Ref<MediaSample>&&, TrackID) final;
    bool isReadyForMoreSamples(TrackID) final;
    void notifyClientWhenReadyForMoreSamples(TrackID) final;
    void allSamplesInTrackEnqueued(TrackID) final;

    void readInitializationSegment();
    void readSamples();

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    ASCIILiteral logClassName() const final { return "SourceBufferPrivateTrinity"_s; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
    const Logger& sourceBufferLogger() const final { return m_logger.get(); }
    uint64_t sourceBufferLogIdentifier() final { return logIdentifier(); }
#endif

    int m_demux { 0 };
    int m_inits { 0 };
    HashMap<TrackID, Ref<TrinityTrackConfig>> m_configs;
    HashSet<TrackID> m_waiting;
#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SourceBufferPrivateTrinity)
static bool isType(const WebCore::SourceBufferPrivate& buffer) { return buffer.platformType() == WebCore::MediaPlatformType::Trinity; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(MEDIA_SOURCE) && USE(TRINITY)
