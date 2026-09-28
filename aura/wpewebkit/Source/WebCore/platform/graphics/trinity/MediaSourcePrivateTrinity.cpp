// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "MediaSourcePrivateTrinity.h"

#if ENABLE(MEDIA_SOURCE) && USE(TRINITY)

#include "ContentType.h"
#include "Logging.h"
#include "MediaPlayerPrivateTrinity.h"
#include "SourceBufferPrivateTrinity.h"

namespace WebCore {

Ref<MediaSourcePrivateTrinity> MediaSourcePrivateTrinity::create(MediaPlayerPrivateTrinity& player, MediaSourcePrivateClient& client)
{
    Ref source = adoptRef(*new MediaSourcePrivateTrinity(player, client));
    client.setPrivateAndOpen(source.copyRef());
    return source;
}

MediaSourcePrivateTrinity::MediaSourcePrivateTrinity(MediaPlayerPrivateTrinity& player, MediaSourcePrivateClient& client)
    : MediaSourcePrivate(client)
    , m_player(player)
#if !RELEASE_LOG_DISABLED
    , m_logger(player.mediaPlayerLogger())
    , m_logIdentifier(player.mediaPlayerLogIdentifier())
#endif
{
}

MediaSourcePrivateTrinity::~MediaSourcePrivateTrinity() = default;

MediaSourcePrivate::AddStatus MediaSourcePrivateTrinity::addSourceBuffer(const ContentType& contentType, const MediaSourceConfiguration&, RefPtr<SourceBufferPrivate>& outPrivate)
{
    MediaEngineSupportParameters parameters {
        .platformType = PlatformMediaDecodingType::MediaSource,
        .type = contentType
    };
    if (MediaPlayerPrivateTrinity::supportsType(parameters) == MediaPlayer::SupportsType::IsNotSupported)
        return AddStatus::NotSupported;
    outPrivate = SourceBufferPrivateTrinity::create(*this);
    {
        Locker locker { m_lock };
        m_sourceBuffers.append(outPrivate);
    }
    outPrivate->setMediaSourceDuration(duration());
    return AddStatus::Ok;
}

RefPtr<MediaPlayerPrivateInterface> MediaSourcePrivateTrinity::player() const
{
    return m_player.get();
}

RefPtr<MediaPlayerPrivateTrinity> MediaSourcePrivateTrinity::trinityPlayer() const
{
    return m_player.get();
}

void MediaSourcePrivateTrinity::setPlayer(MediaPlayerPrivateInterface* player)
{
    m_player = downcast<MediaPlayerPrivateTrinity>(player);
}

void MediaSourcePrivateTrinity::durationChanged(const MediaTime& duration)
{
    MediaSourcePrivate::durationChanged(duration);
    if (RefPtr player = m_player.get())
        player->mediaSourceDurationChanged();
}

void MediaSourcePrivateTrinity::notifyActiveSourceBuffersChanged()
{
    if (RefPtr player = m_player.get())
        player->activeSourceBuffersChanged();
}

void MediaSourcePrivateTrinity::provideWaitingSamples()
{
    for (Ref buffer : sourceBuffers())
        downcast<SourceBufferPrivateTrinity>(buffer.get()).provideWaitingSamples();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaSourcePrivateTrinity::logChannel() const
{
    return LogMediaSource;
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE) && USE(TRINITY)
