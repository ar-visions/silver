// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#if ENABLE(VIDEO) && USE(TRINITY)

#include "MediaPlayerPrivate.h"
#include "PlatformMediaResourceLoader.h"
#include "PlatformTimeRanges.h"
#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedPlatformLayerBufferProxy.h"
#endif
#include <wtf/MonotonicTime.h>
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class MediaSampleTrinity;
class MediaSourcePrivateTrinity;
class VideoFrameTrinity;
class TrinityTrackConfig;

// an mp4 file or a media source, played by trinity
class MediaPlayerPrivateTrinity final
    : public MediaPlayerPrivateInterface
    , public CanMakeWeakPtr<MediaPlayerPrivateTrinity>
    , public RefCounted<MediaPlayerPrivateTrinity> {
    WTF_MAKE_TZONE_ALLOCATED(MediaPlayerPrivateTrinity);
public:
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    explicit MediaPlayerPrivateTrinity(MediaPlayer&);
    ~MediaPlayerPrivateTrinity();

    constexpr MediaPlayerType mediaPlayerType() const final { return MediaPlayerType::Trinity; }
    static void registerMediaEngine(MediaEngineRegistrar);
    static void getSupportedTypes(HashSet<String>&);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    void load(const String&) final;
#if ENABLE(MEDIA_SOURCE)
    void load(const URL&, const LoadOptions&, MediaSourcePrivateClient&) final;
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) final;
#endif
    void cancelLoad() final;

    void play() final;
    void pause() final;
    bool paused() const final { return m_paused; }
    void setRate(float) final;
    void setVolume(float) final;
    void setMuted(bool) final;
    double rate() const final { return m_rate; }

    FloatSize naturalSize() const final { return m_size; }
    bool hasVideo() const final { return m_video; }
    bool hasAudio() const final { return m_hasAudio; }
#if USE(COORDINATED_GRAPHICS)
    // pictures go to their own layer: a new one repaints nothing
    PlatformLayer* platformLayer() const final;
    bool supportsAcceleratedRendering() const final { return true; }
    void acceleratedRenderingStateChanged() final;
#endif
    void setPageIsVisible(bool) final { }

    MediaTime duration() const final;
    MediaTime currentTime() const final;
    Ref<MediaTimePromise> seekToTarget(const SeekTarget&) final;
    MediaTime maxTimeSeekable() const final { return duration(); }
    bool timeIsProgressing() const final;

    MediaPlayer::NetworkState networkState() const final { return m_networkState; }
    MediaPlayer::ReadyState readyState() const final;
    const PlatformTimeRanges& buffered() const final;
    bool didLoadingProgress() const final;
    MediaPlayer::MovieLoadType movieLoadType() const final { return MediaPlayer::MovieLoadType::Download; }

    void setPresentationSize(const IntSize&) final { }
    void paint(GraphicsContext&, const FloatRect&) final;
    RefPtr<VideoFrame> videoFrameForCurrentTime() final;
    DestinationColorSpace colorSpace() final;

    // the loader's calls, on the main thread
    void received(std::span<const uint8_t>);
    void finished();
    void failed();

#if ENABLE(MEDIA_SOURCE)
    // the media source's calls
    void readyStateFromMediaSourceChanged() final;
    void characteristicsFromMediaSourceChanged() final;
    void mediaSourceHasRetrievedAllData() final;
    void mediaSourceDurationChanged();
    void activeSourceBuffersChanged();
    void enqueue(const MediaSampleTrinity&);
    bool readyForMore(uint8_t kind) const;
    void flush(uint8_t kind);
    void videoEnded();
#endif
#if !RELEASE_LOG_DISABLED
    const Logger& mediaPlayerLogger();
    uint64_t mediaPlayerLogIdentifier();
#endif

private:
    void setNetworkState(MediaPlayer::NetworkState);
    void setReadyState(MediaPlayer::ReadyState);
    double position() const;
    void tick();
    void showFrame(double seconds);
    void applyVolume();

    ThreadSafeWeakPtr<MediaPlayer> m_player;
    RefPtr<PlatformMediaResourceLoader> m_loader;
    RefPtr<PlatformMediaResource> m_resource;
    Vector<uint8_t> m_bytes;
    bool m_progressed { false };
    int m_video { 0 };
    bool m_hasAudio { false };
    float m_volume { 1 };
    bool m_muted { false };
    FloatSize m_size;
    MediaTime m_duration { MediaTime::zeroTime() };
    MediaPlayer::NetworkState m_networkState { MediaPlayer::NetworkState::Empty };
    MediaPlayer::ReadyState m_readyState { MediaPlayer::ReadyState::HaveNothing };
    PlatformTimeRanges m_buffered;
    bool m_paused { true };
    double m_rate { 1 };
    double m_from { 0 };
    MonotonicTime m_started;
    RunLoop::Timer m_timer;
    RefPtr<VideoFrameTrinity> m_frame;
#if USE(COORDINATED_GRAPHICS)
    RefPtr<CoordinatedPlatformLayerBufferProxy> m_contentsBufferProxy;
    bool m_accelerated { false };
    bool pushPicture(double seconds);
#endif
#if ENABLE(MEDIA_SOURCE)
    void tickSource();
    double sourcePosition() const;
    RefPtr<MediaSourcePrivateTrinity> m_mediaSource;
    RefPtr<const TrinityTrackConfig> m_videoConfig;
    RefPtr<const TrinityTrackConfig> m_audioConfig;
    std::optional<SeekTarget> m_seekTarget;
    std::optional<MediaTimePromise::AutoRejectProducer> m_seekPromise;
    mutable PlatformTimeRanges m_sourceBuffered;
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MediaPlayerPrivateTrinity)
static bool isType(const WebCore::MediaPlayerPrivateInterface& player) { return player.mediaPlayerType() == WebCore::MediaPlayerType::Trinity; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(VIDEO) && USE(TRINITY)
