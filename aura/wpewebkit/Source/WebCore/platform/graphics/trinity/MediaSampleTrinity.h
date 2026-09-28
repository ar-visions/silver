// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#if ENABLE(MEDIA_SOURCE) && USE(TRINITY)

#include "MediaSample.h"
#include "SharedBuffer.h"
#include <wtf/PrintStream.h>
#include <wtf/Vector.h>

namespace WebCore {

// a track's decoder setup: avcC or the AudioSpecificConfig
class TrinityTrackConfig final : public ThreadSafeRefCounted<TrinityTrackConfig> {
public:
    enum class Kind : uint8_t { Video = 1, Audio = 2 };

    static Ref<TrinityTrackConfig> create(Kind kind, Vector<uint8_t>&& bytes, FloatSize size)
    {
        return adoptRef(*new TrinityTrackConfig(kind, WTF::move(bytes), size));
    }

    Kind kind() const { return m_kind; }
    std::span<const uint8_t> bytes() const { return m_bytes.span(); }
    FloatSize size() const { return m_size; }

private:
    TrinityTrackConfig(Kind kind, Vector<uint8_t>&& bytes, FloatSize size)
        : m_kind(kind)
        , m_bytes(WTF::move(bytes))
        , m_size(size)
    {
    }

    Kind m_kind;
    Vector<uint8_t> m_bytes;
    FloatSize m_size;
};

// one mp4 sample: its bytes, times, and its track's setup
class MediaSampleTrinity final : public MediaSample {
public:
    static Ref<MediaSampleTrinity> create(TrackID track, Ref<SharedBuffer>&& data, MediaTime presentation, MediaTime decode, MediaTime duration, bool sync, Ref<TrinityTrackConfig>&& config)
    {
        return adoptRef(*new MediaSampleTrinity(track, WTF::move(data), presentation, decode, duration, sync ? IsSync : None, WTF::move(config)));
    }

    std::span<const uint8_t> bytes() const { return m_data->span(); }
    const TrinityTrackConfig& config() const { return m_config.get(); }

    MediaTime presentationTime() const final { return m_presentation; }
    MediaTime decodeTime() const final { return m_decode; }
    MediaTime duration() const final { return m_duration; }
    TrackID trackID() const final { return m_track; }
    size_t sizeInBytes() const final { return m_data->size(); }
    FloatSize presentationSize() const final { return m_config->size(); }
    void offsetTimestampsBy(const MediaTime& offset) final
    {
        m_presentation += offset;
        m_decode += offset;
    }
    void setTimestamps(const MediaTime& presentation, const MediaTime& decode) final
    {
        m_presentation = presentation;
        m_decode = decode;
    }
    Ref<MediaSample> createNonDisplayingCopy() const final
    {
        return adoptRef(*new MediaSampleTrinity(m_track, m_data.copyRef(), m_presentation, m_decode, m_duration, static_cast<SampleFlags>(m_flags | IsNonDisplaying), m_config.copyRef()));
    }
    SampleFlags flags() const final { return m_flags; }
    PlatformSample platformSample() const final { return PlatformSample { static_cast<const MockSampleBox*>(nullptr) }; }
    Type type() const final { return Type::None; }
    void dump(PrintStream& out) const final
    {
        out.print("{PTS(", m_presentation, "), DTS(", m_decode, "), duration(", m_duration, "), flags(", static_cast<int>(m_flags), ")}");
    }

private:
    MediaSampleTrinity(TrackID track, Ref<SharedBuffer>&& data, MediaTime presentation, MediaTime decode, MediaTime duration, SampleFlags flags, Ref<TrinityTrackConfig>&& config)
        : m_track(track)
        , m_data(WTF::move(data))
        , m_presentation(presentation)
        , m_decode(decode)
        , m_duration(duration)
        , m_flags(flags)
        , m_config(WTF::move(config))
    {
    }

    TrackID m_track;
    Ref<SharedBuffer> m_data;
    MediaTime m_presentation;
    MediaTime m_decode;
    MediaTime m_duration;
    SampleFlags m_flags;
    Ref<TrinityTrackConfig> m_config;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE) && USE(TRINITY)
