// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "SourceBufferPrivateTrinity.h"

#if ENABLE(MEDIA_SOURCE) && USE(TRINITY)

#include "AudioTrackPrivate.h"
#include "ContentType.h"
#include "Logging.h"
#include "MediaDescription.h"
#include "MediaPlayerPrivateTrinity.h"
#include "MediaSampleTrinity.h"
#include "MediaSourcePrivateTrinity.h"
#include "SharedBuffer.h"
#include "SourceBufferPrivateClient.h"
#include "VideoTrackPrivate.h"
#include "WebGfx.h"
#include <wtf/NativePromise.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

class TrinityVideoTrack final : public VideoTrackPrivate {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(TrinityVideoTrack);
public:
    static Ref<TrinityVideoTrack> create(TrackID id) { return adoptRef(*new TrinityVideoTrack(id)); }
    TrackID id() const final { return m_id; }

private:
    explicit TrinityVideoTrack(TrackID id)
        : m_id(id)
    {
    }
    TrackID m_id;
};

class TrinityAudioTrack final : public AudioTrackPrivate {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(TrinityAudioTrack);
public:
    static Ref<TrinityAudioTrack> create(TrackID id) { return adoptRef(*new TrinityAudioTrack(id)); }
    TrackID id() const final { return m_id; }

private:
    explicit TrinityAudioTrack(TrackID id)
        : m_id(id)
    {
    }
    TrackID m_id;
};

// the sample entry's four bytes: SourceBuffer's codec
class TrinityMediaDescription final : public MediaDescription {
public:
    static Ref<TrinityMediaDescription> create(String&& codec, bool video) { return adoptRef(*new TrinityMediaDescription(WTF::move(codec), video)); }
    bool isVideo() const final { return m_video; }
    bool isAudio() const final { return !m_video; }
    bool isText() const final { return false; }

private:
    TrinityMediaDescription(String&& codec, bool video)
        : MediaDescription(WTF::move(codec))
        , m_video(video)
    {
    }
    bool m_video;
};

static String fourCharacters(int64_t code)
{
    std::array<Latin1Character, 4> text {
        static_cast<Latin1Character>((code >> 24) & 0xff), static_cast<Latin1Character>((code >> 16) & 0xff),
        static_cast<Latin1Character>((code >> 8) & 0xff), static_cast<Latin1Character>(code & 0xff)
    };
    return String(std::span<const Latin1Character> { text });
}

Ref<SourceBufferPrivateTrinity> SourceBufferPrivateTrinity::create(MediaSourcePrivateTrinity& parent)
{
    return adoptRef(*new SourceBufferPrivateTrinity(parent));
}

SourceBufferPrivateTrinity::SourceBufferPrivateTrinity(MediaSourcePrivateTrinity& parent)
    : SourceBufferPrivate(parent)
    , m_demux(webgfx_demux_new())
#if !RELEASE_LOG_DISABLED
    , m_logger(parent.logger())
    , m_logIdentifier(parent.nextSourceBufferLogIdentifier())
#endif
{
}

SourceBufferPrivateTrinity::~SourceBufferPrivateTrinity()
{
    webgfx_demux_free(m_demux);
}

RefPtr<MediaSourcePrivateTrinity> SourceBufferPrivateTrinity::mediaSourcePrivate() const
{
    return downcast<MediaSourcePrivateTrinity>(m_mediaSource.get());
}

Ref<MediaPromise> SourceBufferPrivateTrinity::appendInternal(Ref<SharedBuffer>&& data)
{
    auto bytes = data->span();
    if (!webgfx_demux_append(m_demux, bytes.data(), static_cast<int64_t>(bytes.size())))
        return MediaPromise::createAndReject(PlatformMediaError::ParsingError);
    if (webgfx_demux_inits(m_demux) != m_inits) {
        m_inits = webgfx_demux_inits(m_demux);
        readInitializationSegment();
    }
    readSamples();
    return MediaPromise::createAndResolve();
}

void SourceBufferPrivateTrinity::readInitializationSegment()
{
    SourceBufferPrivateClient::InitializationSegment segment;
    segment.duration = MediaTime::invalidTime();
    int tracks = webgfx_demux_tracks(m_demux);
    for (int i = 0; i < tracks; ++i) {
        std::array<int64_t, 8> info { };
        webgfx_demux_track(m_demux, i, info.data());
        auto id = static_cast<TrackID>(info[0]);
        Vector<uint8_t> config(static_cast<size_t>(std::max(webgfx_demux_track_config(m_demux, i, nullptr, 0), 0)));
        webgfx_demux_track_config(m_demux, i, config.mutableSpan().data(), static_cast<int>(config.size()));
        auto codec = fourCharacters(info[2]);
        if (info[1] == 1) {
            FloatSize size(info[4], info[5]);
            m_configs.set(id, TrinityTrackConfig::create(TrinityTrackConfig::Kind::Video, WTF::move(config), size));
            segment.videoTracks.append({ TrinityMediaDescription::create(WTF::move(codec), true), TrinityVideoTrack::create(id) });
        } else if (info[1] == 2) {
            m_configs.set(id, TrinityTrackConfig::create(TrinityTrackConfig::Kind::Audio, WTF::move(config), { }));
            segment.audioTracks.append({ TrinityMediaDescription::create(WTF::move(codec), false), TrinityAudioTrack::create(id) });
        }
    }
    SourceBufferPrivate::didReceiveInitializationSegment(WTF::move(segment));
}

void SourceBufferPrivateTrinity::readSamples()
{
    int count = webgfx_demux_samples(m_demux);
    for (int i = 0; i < count; ++i) {
        std::array<int64_t, 6> info { };
        int64_t size = webgfx_demux_sample(m_demux, i, info.data());
        auto track = static_cast<TrackID>(info[0]);
        auto config = m_configs.find(track);
        if (size < 0 || config == m_configs.end() || info[4] <= 0)
            continue;
        Vector<uint8_t> bytes(static_cast<size_t>(size));
        webgfx_demux_sample_data(m_demux, i, bytes.mutableSpan().data());
        auto sample = MediaSampleTrinity::create(track, SharedBuffer::create(WTF::move(bytes)),
            MediaTime(info[2], info[4]), MediaTime(info[1], info[4]), MediaTime(info[3], info[4]), info[5], config->value.copyRef());
        SourceBufferPrivate::didReceiveSample(WTF::move(sample));
    }
    webgfx_demux_clear(m_demux);
}

void SourceBufferPrivateTrinity::resetParserStateInternal()
{
    webgfx_demux_reset(m_demux);
}

bool SourceBufferPrivateTrinity::canSwitchToType(const ContentType& contentType)
{
    MediaEngineSupportParameters parameters {
        .platformType = PlatformMediaDecodingType::MediaSource,
        .type = contentType
    };
    return MediaPlayerPrivateTrinity::supportsType(parameters) != MediaPlayer::SupportsType::IsNotSupported;
}

static TrinityTrackConfig::Kind kindOf(const HashMap<TrackID, Ref<TrinityTrackConfig>>& configs, TrackID track)
{
    auto it = configs.find(track);
    return it == configs.end() ? TrinityTrackConfig::Kind::Video : it->value->kind();
}

void SourceBufferPrivateTrinity::flush(TrackID track)
{
    if (RefPtr source = mediaSourcePrivate()) {
        if (RefPtr player = source->trinityPlayer())
            player->flush(static_cast<uint8_t>(kindOf(m_configs, track)));
    }
}

void SourceBufferPrivateTrinity::enqueueSample(Ref<MediaSample>&& sample, TrackID)
{
    if (sample->type() != MediaSample::Type::None)
        return;
    RefPtr source = mediaSourcePrivate();
    if (!source)
        return;
    if (RefPtr player = source->trinityPlayer())
        player->enqueue(static_cast<MediaSampleTrinity&>(sample.get()));
}

bool SourceBufferPrivateTrinity::isReadyForMoreSamples(TrackID track)
{
    RefPtr source = mediaSourcePrivate();
    RefPtr player = source ? source->trinityPlayer() : nullptr;
    return player && player->readyForMore(static_cast<uint8_t>(kindOf(m_configs, track)));
}

void SourceBufferPrivateTrinity::notifyClientWhenReadyForMoreSamples(TrackID track)
{
    m_waiting.add(track);
}

void SourceBufferPrivateTrinity::provideWaitingSamples()
{
    for (auto track : copyToVector(m_waiting)) {
        if (!isReadyForMoreSamples(track))
            continue;
        m_waiting.remove(track);
        provideMediaData(track);
    }
}

void SourceBufferPrivateTrinity::allSamplesInTrackEnqueued(TrackID track)
{
    if (kindOf(m_configs, track) != TrinityTrackConfig::Kind::Video)
        return;
    if (RefPtr source = mediaSourcePrivate()) {
        if (RefPtr player = source->trinityPlayer())
            player->videoEnded();
    }
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBufferPrivateTrinity::logChannel() const
{
    return LogMediaSource;
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE) && USE(TRINITY)
