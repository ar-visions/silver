// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "MediaPlayerPrivateTrinity.h"

#if ENABLE(VIDEO) && USE(TRINITY)

#include "GraphicsContext.h"
#include "MediaSampleTrinity.h"
#include "MediaSourcePrivateClient.h"
#include "MediaSourcePrivateTrinity.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "SharedBuffer.h"
#include "VideoFrameTrinity.h"
#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedPlatformLayerBufferTrinity.h"
#endif
#include "WebGfx.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaPlayerPrivateTrinity);

// the network's bytes into the player, on the main thread
class TrinityMediaClient final : public PlatformMediaResourceClient {
public:
    static Ref<TrinityMediaClient> create(MediaPlayerPrivateTrinity& player) { return adoptRef(*new TrinityMediaClient(player)); }

private:
    explicit TrinityMediaClient(MediaPlayerPrivateTrinity& player)
        : m_player(player)
    {
    }

    void dataReceived(PlatformMediaResource&, const SharedBuffer& buffer) final
    {
        if (RefPtr player = m_player.get())
            player->received(buffer.span());
    }

    void loadFinished(PlatformMediaResource&, const NetworkLoadMetrics&) final
    {
        if (RefPtr player = m_player.get())
            player->finished();
    }

    void loadFailed(PlatformMediaResource&, const ResourceError&) final
    {
        if (RefPtr player = m_player.get())
            player->failed();
    }

    void accessControlCheckFailed(PlatformMediaResource&, const ResourceError&) final
    {
        if (RefPtr player = m_player.get())
            player->failed();
    }

    WeakPtr<MediaPlayerPrivateTrinity> m_player;
};

// the picture ticks at the screen's refresh (TRINITY_VIDEO_HZ from the element), else 60
static double videoHz()
{
    static double hz = [] {
        const char* e = getenv("TRINITY_VIDEO_HZ");
        double v = e ? atof(e) : 0;
        return v >= 24 && v <= 480 ? v : 60.0;
    }();
    return hz;
}

MediaPlayerPrivateTrinity::MediaPlayerPrivateTrinity(MediaPlayer& player)
    : m_player(player)
    , m_timer(RunLoop::mainSingleton(), "MediaPlayerPrivateTrinity::Timer"_s, this, &MediaPlayerPrivateTrinity::tick)
{
#if USE(COORDINATED_GRAPHICS)
    m_contentsBufferProxy = CoordinatedPlatformLayerBufferProxy::create();
#endif
}

#if USE(COORDINATED_GRAPHICS)
PlatformLayer* MediaPlayerPrivateTrinity::platformLayer() const
{
    return m_contentsBufferProxy.get();
}

void MediaPlayerPrivateTrinity::acceleratedRenderingStateChanged()
{
    RefPtr player = m_player.get();
    m_accelerated = player && player->renderingCanBeAccelerated();
}

// the picture due at seconds, into the video's layer; true when new
bool MediaPlayerPrivateTrinity::pushPicture(double seconds)
{
    int width = 0, height = 0;
    if (!webgfx_stream_next(m_video, std::llround(seconds * 1e6), &width, &height))
        return false;
    Vector<uint8_t> y(width * height), u((width / 2) * (height / 2)), v((width / 2) * (height / 2));
    webgfx_stream_planes(m_video, y.mutableSpan().data(), u.mutableSpan().data(), v.mutableSpan().data());
    IntSize size(width, height);
    m_contentsBufferProxy->setDisplayBuffer(CoordinatedPlatformLayerBufferTrinity::create(size, height >= 720, WTF::move(y), WTF::move(u), WTF::move(v)));
    if (FloatSize(size) != m_size) {
        m_size = FloatSize(size);
        if (RefPtr player = m_player.get())
            player->sizeChanged();
    }
    return true;
}
#endif

MediaPlayerPrivateTrinity::~MediaPlayerPrivateTrinity()
{
    cancelLoad();
    if (m_video)
        webgfx_video_free(m_video);
}

static HashSet<String>& mimeTypes()
{
    static NeverDestroyed<HashSet<String>> types = HashSet<String> { "video/mp4"_s, "video/quicktime"_s, "video/x-m4v"_s, "audio/mp4"_s };
    return types;
}

void MediaPlayerPrivateTrinity::getSupportedTypes(HashSet<String>& types)
{
    types = mimeTypes();
}

// h.264 and aac in mp4, as a file or a media source
MediaPlayer::SupportsType MediaPlayerPrivateTrinity::supportsType(const MediaEngineSupportParameters& parameters)
{
    bool source = parameters.platformType == PlatformMediaDecodingType::MediaSource;
    if (parameters.platformType != PlatformMediaDecodingType::FileOrHLS && !source)
        return MediaPlayer::SupportsType::IsNotSupported;
#if !ENABLE(MEDIA_SOURCE)
    if (source)
        return MediaPlayer::SupportsType::IsNotSupported;
#endif
    auto container = parameters.type.containerType();
    // a media source attaches with no type; a null key crashes
    if (container.isEmpty())
        return source ? MediaPlayer::SupportsType::MayBeSupported : MediaPlayer::SupportsType::IsNotSupported;
    if (!mimeTypes().contains(container))
        return MediaPlayer::SupportsType::IsNotSupported;
    auto codecs = parameters.type.codecs();
    if (codecs.isEmpty())
        return MediaPlayer::SupportsType::MayBeSupported;
    for (auto& codec : codecs) {
        if (!codec.startsWith("avc1"_s) && !codec.startsWith("avc3"_s) && !codec.startsWith("mp4a"_s))
            return MediaPlayer::SupportsType::IsNotSupported;
    }
    return MediaPlayer::SupportsType::IsSupported;
}

class MediaPlayerFactoryTrinity final : public MediaPlayerFactory {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(MediaPlayerFactoryTrinity);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MediaPlayerFactoryTrinity);
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::Trinity; }
    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer& player) const final { return adoptRef(*new MediaPlayerPrivateTrinity(player)); }
    void getSupportedTypes(HashSet<String>& types) const final { MediaPlayerPrivateTrinity::getSupportedTypes(types); }
    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final { return MediaPlayerPrivateTrinity::supportsType(parameters); }
};

void MediaPlayerPrivateTrinity::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar(makeUnique<MediaPlayerFactoryTrinity>());
}

void MediaPlayerPrivateTrinity::setNetworkState(MediaPlayer::NetworkState state)
{
    if (m_networkState == state)
        return;
    m_networkState = state;
    if (RefPtr player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateTrinity::setReadyState(MediaPlayer::ReadyState state)
{
    if (m_readyState == state)
        return;
    m_readyState = state;
    if (RefPtr player = m_player.get())
        player->readyStateChanged();
}

void MediaPlayerPrivateTrinity::load(const String& url)
{
    RefPtr player = m_player.get();
    if (!player)
        return;
    setNetworkState(MediaPlayer::NetworkState::Loading);
    setReadyState(MediaPlayer::ReadyState::HaveNothing);
    m_loader = player->mediaResourceLoader();
    m_resource = m_loader->requestResource(ResourceRequest { URL { url } }, PlatformMediaResourceLoader::LoadOption::DisallowCaching);
    if (!m_resource) {
        setNetworkState(MediaPlayer::NetworkState::NetworkError);
        return;
    }
    m_resource->setClient(TrinityMediaClient::create(*this));
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateTrinity::load(const URL&, const LoadOptions&, MediaSourcePrivateClient& client)
{
    if (RefPtr existing = dynamicDowncast<MediaSourcePrivateTrinity>(client.mediaSourcePrivate())) {
        existing->setPlayer(this);
        m_mediaSource = WTF::move(existing);
        client.reOpen();
    } else
        m_mediaSource = MediaSourcePrivateTrinity::create(*this, client);
    m_video = webgfx_stream_new();
    applyVolume();
    setNetworkState(MediaPlayer::NetworkState::Loading);
    m_timer.startRepeating(1_s / videoHz());
}

void MediaPlayerPrivateTrinity::readyStateFromMediaSourceChanged()
{
    if (RefPtr player = m_player.get())
        player->readyStateChanged();
}

void MediaPlayerPrivateTrinity::characteristicsFromMediaSourceChanged()
{
    if (RefPtr player = m_player.get())
        player->characteristicChanged();
}

void MediaPlayerPrivateTrinity::mediaSourceHasRetrievedAllData()
{
    setNetworkState(MediaPlayer::NetworkState::Loaded);
}

void MediaPlayerPrivateTrinity::mediaSourceDurationChanged()
{
    if (RefPtr player = m_player.get())
        player->durationChanged();
}

void MediaPlayerPrivateTrinity::activeSourceBuffersChanged()
{
    if (RefPtr player = m_player.get())
        player->activeSourceBuffersChanged();
}

static int64_t microseconds(const MediaTime& time)
{
    return std::llround(time.toDouble() * 1e6);
}

// a new track setup goes to the decoder before its samples
void MediaPlayerPrivateTrinity::enqueue(const MediaSampleTrinity& sample)
{
    auto& config = sample.config();
    auto bytes = sample.bytes();
    if (config.kind() == TrinityTrackConfig::Kind::Video) {
        if (m_videoConfig != &config) {
            m_videoConfig = &config;
            webgfx_stream_video_config(m_video, config.bytes().data(), static_cast<int>(config.bytes().size()));
            if (m_size != config.size()) {
                m_size = config.size();
                if (RefPtr player = m_player.get())
                    player->sizeChanged();
            }
        }
        webgfx_stream_video_sample(m_video, bytes.data(), static_cast<int64_t>(bytes.size()), microseconds(sample.presentationTime()), sample.isSync(), !sample.isNonDisplaying());
        return;
    }
    if (m_audioConfig != &config) {
        m_audioConfig = &config;
        bool had = m_hasAudio;
        m_hasAudio = webgfx_stream_audio_config(m_video, config.bytes().data(), static_cast<int>(config.bytes().size()));
        applyVolume();
        // the element re-checks autoplay: sound needs a click
        if (m_hasAudio != had) {
            if (RefPtr player = m_player.get())
                player->characteristicChanged();
        }
    }
    // no sound device: the plain clock runs, the samples go unheard
    if (!m_hasAudio)
        return;
    webgfx_stream_audio_sample(m_video, bytes.data(), static_cast<int64_t>(bytes.size()), microseconds(sample.presentationTime()));
}

// room for 16 pictures and 3 seconds of sound
bool MediaPlayerPrivateTrinity::readyForMore(uint8_t kind) const
{
    if (kind == static_cast<uint8_t>(TrinityTrackConfig::Kind::Video))
        return webgfx_stream_video_queued(m_video) < 16;
    return !m_hasAudio || webgfx_stream_audio_buffered(m_video) < 3000000;
}

void MediaPlayerPrivateTrinity::flush(uint8_t kind)
{
    bool video = kind == static_cast<uint8_t>(TrinityTrackConfig::Kind::Video);
    webgfx_stream_flush(m_video, video ? 1 : 2);
    if (video)
        m_videoConfig = nullptr;
    else
        m_audioConfig = nullptr;
}

void MediaPlayerPrivateTrinity::videoEnded()
{
    webgfx_stream_video_end(m_video);
}

// the sound's clock; without sound, the plain one
double MediaPlayerPrivateTrinity::sourcePosition() const
{
    if (m_paused)
        return m_from;
    if (m_hasAudio) {
        int64_t heard = webgfx_stream_time(m_video);
        return heard < 0 ? m_from : heard / 1e6;
    }
    return m_from + (MonotonicTime::now() - m_started).seconds() * m_rate;
}

void MediaPlayerPrivateTrinity::tickSource()
{
    RefPtr source = m_mediaSource;
    source->provideWaitingSamples();
    if (m_seekTarget)
        return;
    double at = sourcePosition();
#if USE(COORDINATED_GRAPHICS)
    if (m_accelerated)
        pushPicture(at);
    else
#endif
    if (webgfx_stream_advance(m_video, std::llround(at * 1e6))) {
        int width = 0, height = 0;
        webgfx_stream_size(m_video, &width, &height);
        RefPtr player = m_player.get();
        if (FloatSize(width, height) != m_size) {
            m_size = FloatSize(width, height);
            if (player)
                player->sizeChanged();
        }
        m_frame = VideoFrameTrinity::create(m_video, IntSize(width, height), MediaTime::createWithDouble(at));
        if (player)
            player->repaint();
    }
    auto end = source->duration();
    if (!m_paused && source->isEnded() && end.isFinite() && at >= end.toDouble()) {
        m_from = end.toDouble();
        m_paused = true;
        webgfx_stream_playing(m_video, 0);
        if (RefPtr player = m_player.get())
            player->timeChanged();
    }
}
#endif

#if !RELEASE_LOG_DISABLED
const Logger& MediaPlayerPrivateTrinity::mediaPlayerLogger()
{
    return m_player.get()->mediaPlayerLogger();
}

uint64_t MediaPlayerPrivateTrinity::mediaPlayerLogIdentifier()
{
    return m_player.get()->mediaPlayerLogIdentifier();
}
#endif

#if ENABLE(MEDIA_STREAM)
void MediaPlayerPrivateTrinity::load(MediaStreamPrivate&)
{
    setNetworkState(MediaPlayer::NetworkState::FormatError);
}
#endif

void MediaPlayerPrivateTrinity::cancelLoad()
{
    if (m_resource) {
        m_resource->setClient(nullptr);
        m_resource->shutdown();
        m_resource = nullptr;
    }
    m_timer.stop();
}

void MediaPlayerPrivateTrinity::received(std::span<const uint8_t> bytes)
{
    m_bytes.append(bytes);
    m_progressed = true;
}

// progressive mp4 plays once the whole file is here
void MediaPlayerPrivateTrinity::finished()
{
    m_resource = nullptr;
    m_video = webgfx_video_new(m_bytes.span().data(), static_cast<int64_t>(m_bytes.size()));
    m_bytes.clear();
    m_bytes.shrinkToFit();
    if (!m_video) {
        setNetworkState(MediaPlayer::NetworkState::FormatError);
        return;
    }
    int width = 0, height = 0;
    double seconds = 0;
    webgfx_video_info(m_video, &width, &height, &seconds);
    m_size = FloatSize(width, height);
    m_hasAudio = webgfx_video_has_audio(m_video);
    applyVolume();
    m_duration = MediaTime::createWithDouble(seconds);
    m_buffered.add(MediaTime::zeroTime(), m_duration);
    showFrame(0);
    RefPtr player = m_player.get();
    if (player) {
        player->characteristicChanged();
        player->sizeChanged();
        player->durationChanged();
    }
    setReadyState(MediaPlayer::ReadyState::HaveMetadata);
    setReadyState(MediaPlayer::ReadyState::HaveEnoughData);
    setNetworkState(MediaPlayer::NetworkState::Loaded);
}

void MediaPlayerPrivateTrinity::failed()
{
    m_resource = nullptr;
    setNetworkState(MediaPlayer::NetworkState::NetworkError);
}

MediaTime MediaPlayerPrivateTrinity::duration() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource)
        return m_mediaSource->duration();
#endif
    return m_duration;
}

MediaPlayer::ReadyState MediaPlayerPrivateTrinity::readyState() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource)
        return m_mediaSource->mediaPlayerReadyState();
#endif
    return m_readyState;
}

const PlatformTimeRanges& MediaPlayerPrivateTrinity::buffered() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        m_sourceBuffered = m_mediaSource->buffered();
        return m_sourceBuffered;
    }
#endif
    return m_buffered;
}

bool MediaPlayerPrivateTrinity::timeIsProgressing() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource)
        return !m_paused && !m_seekTarget && m_mediaSource->hasFutureTime(currentTime());
#endif
    return !m_paused;
}

bool MediaPlayerPrivateTrinity::didLoadingProgress() const
{
    bool progressed = m_progressed;
    const_cast<MediaPlayerPrivateTrinity*>(this)->m_progressed = false;
    return progressed;
}

double MediaPlayerPrivateTrinity::position() const
{
    if (m_paused)
        return m_from;
    double at = m_from + (MonotonicTime::now() - m_started).seconds() * m_rate;
    return std::min(at, m_duration.toDouble());
}

MediaTime MediaPlayerPrivateTrinity::currentTime() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        if (m_seekTarget)
            return m_seekTarget->time;
        return MediaTime::createWithDouble(sourcePosition());
    }
#endif
    return MediaTime::createWithDouble(position());
}

void MediaPlayerPrivateTrinity::play()
{
    if (!m_paused)
        return;
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        m_paused = false;
        m_started = MonotonicTime::now();
        webgfx_stream_playing(m_video, 1);
        if (RefPtr player = m_player.get())
            player->playbackStateChanged();
        return;
    }
#endif
    m_paused = false;
    m_started = MonotonicTime::now();
    m_timer.startRepeating(1_s / videoHz());
    if (m_video)
        webgfx_video_play(m_video, m_from);
    if (RefPtr player = m_player.get())
        player->playbackStateChanged();
}

void MediaPlayerPrivateTrinity::pause()
{
    if (m_paused)
        return;
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        m_from = sourcePosition();
        m_paused = true;
        webgfx_stream_playing(m_video, 0);
        if (RefPtr player = m_player.get())
            player->playbackStateChanged();
        return;
    }
#endif
    m_from = position();
    m_paused = true;
    m_timer.stop();
    if (m_video)
        webgfx_video_pause(m_video);
    if (RefPtr player = m_player.get())
        player->playbackStateChanged();
}

void MediaPlayerPrivateTrinity::setRate(float rate)
{
    // a media source's clock is sourcePosition, not the file's
    m_from = m_mediaSource ? sourcePosition() : position();
    m_started = MonotonicTime::now();
    m_rate = rate;
}

void MediaPlayerPrivateTrinity::setVolume(float volume)
{
    m_volume = volume;
    applyVolume();
}

void MediaPlayerPrivateTrinity::setMuted(bool muted)
{
    m_muted = muted;
    applyVolume();
}

void MediaPlayerPrivateTrinity::applyVolume()
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        webgfx_stream_gain(m_video, m_muted ? 0 : m_volume);
        return;
    }
#endif
    if (m_video)
        webgfx_video_volume(m_video, m_muted ? 0 : m_volume);
}

void MediaPlayerPrivateTrinity::showFrame(double seconds)
{
    if (!m_video || !webgfx_video_advance(m_video, seconds))
        return;
    m_frame = VideoFrameTrinity::create(m_video, IntSize(m_size.width(), m_size.height()), MediaTime::createWithDouble(seconds));
    if (RefPtr player = m_player.get())
        player->repaint();
}

void MediaPlayerPrivateTrinity::tick()
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        tickSource();
        return;
    }
#endif
    double at = position();
    showFrame(at);
    RefPtr player = m_player.get();
    if (at >= m_duration.toDouble()) {
        m_from = m_duration.toDouble();
        m_paused = true;
        m_timer.stop();
        webgfx_video_pause(m_video);
        if (player)
            player->timeChanged();
    }
}

Ref<MediaTimePromise> MediaPlayerPrivateTrinity::seekToTarget(const SeekTarget& target)
{
#if ENABLE(MEDIA_SOURCE)
    // the buffers refill from the key frame before the time
    if (RefPtr source = m_mediaSource) {
        m_seekTarget = target;
        m_seekPromise.emplace(PlatformMediaError::Cancelled);
        source->waitForTarget(target)->whenSettled(RunLoop::currentSingleton(), [weakThis = WeakPtr { *this }](auto&& result) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            if (!result) {
                protectedThis->m_seekTarget.reset();
                if (auto promise = std::exchange(protectedThis->m_seekPromise, std::nullopt))
                    promise->reject(result.error());
                return;
            }
            auto time = *result;
            protect(protectedThis->m_mediaSource)->reenqueueMediaForTime(time)->whenSettled(RunLoop::currentSingleton(), [weakThis, time](auto&& result) {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis || !result)
                    return;
                protectedThis->m_seekTarget.reset();
                protectedThis->m_from = time.toDouble();
                protectedThis->m_started = MonotonicTime::now();
                if (auto promise = std::exchange(protectedThis->m_seekPromise, std::nullopt))
                    promise->resolve(time);
            });
        });
        return *m_seekPromise;
    }
#endif
    double to = std::clamp(target.time.toDouble(), 0.0, m_duration.toDouble());
    m_from = to;
    m_started = MonotonicTime::now();
    if (m_video) {
        webgfx_video_seek(m_video, to);
        showFrame(to);
        if (!m_paused)
            webgfx_video_play(m_video, to);
    }
    if (RefPtr player = m_player.get())
        player->timeChanged();
    return MediaTimePromise::createAndResolve(MediaTime::createWithDouble(to));
}

void MediaPlayerPrivateTrinity::paint(GraphicsContext& context, const FloatRect& rect)
{
    if (context.paintingDisabled() || !m_frame)
        return;
    context.drawVideoFrame(*m_frame, rect, ImageOrientation::Orientation::None, true);
}

RefPtr<VideoFrame> MediaPlayerPrivateTrinity::videoFrameForCurrentTime()
{
    return m_frame;
}

DestinationColorSpace MediaPlayerPrivateTrinity::colorSpace()
{
    return DestinationColorSpace::SRGB();
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(TRINITY)
