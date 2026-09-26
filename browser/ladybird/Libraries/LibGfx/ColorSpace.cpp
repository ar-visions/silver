/*
 * Copyright (c) 2024, Lucas Chollet <lucas.chollet@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteBuffer.h>
#include <AK/Types.h>
#include <LibGfx/ColorSpace.h>
#include <LibIPC/Decoder.h>
#include <LibIPC/Encoder.h>

namespace Gfx {

namespace Details {

// kept as data; images are drawn as sRGB until trinity converts
struct ColorSpaceImpl {
    AK_ALLOC_WITH_KMALLOC;

    ByteBuffer description;
};

}

enum class DescriptionTag : u8 {
    Cicp = 1,
    Icc = 2,
};

ColorSpace::ColorSpace()
    : m_color_space(make<Details::ColorSpaceImpl>())
{
}

ColorSpace::ColorSpace(ColorSpace const& other)
    : m_color_space(make<Details::ColorSpaceImpl>(MUST(ByteBuffer::copy(other.m_color_space->description))))
{
}

ColorSpace& ColorSpace::operator=(ColorSpace const& other)
{
    if (this != &other)
        m_color_space = make<Details::ColorSpaceImpl>(MUST(ByteBuffer::copy(other.m_color_space->description)));
    return *this;
}

ColorSpace::ColorSpace(ColorSpace&& other) = default;
ColorSpace& ColorSpace::operator=(ColorSpace&&) = default;
ColorSpace::~ColorSpace() = default;

ColorSpace::ColorSpace(NonnullOwnPtr<Details::ColorSpaceImpl>&& color_space)
    : m_color_space(move(color_space))
{
}

ErrorOr<ColorSpace> ColorSpace::from_cicp(Media::CodingIndependentCodePoints cicp)
{
    auto description = TRY(ByteBuffer::create_uninitialized(5));
    description[0] = to_underlying(DescriptionTag::Cicp);
    description[1] = to_underlying(cicp.color_primaries());
    description[2] = to_underlying(cicp.transfer_characteristics());
    description[3] = to_underlying(cicp.matrix_coefficients());
    description[4] = to_underlying(cicp.video_full_range_flag());
    return ColorSpace { make<Details::ColorSpaceImpl>(move(description)) };
}

ErrorOr<ColorSpace> ColorSpace::load_from_icc_bytes(ReadonlyBytes icc_bytes)
{
    if (icc_bytes.is_empty())
        return ColorSpace {};
    auto description = TRY(ByteBuffer::create_uninitialized(icc_bytes.size() + 1));
    description[0] = to_underlying(DescriptionTag::Icc);
    icc_bytes.copy_to(description.bytes().slice(1));
    return ColorSpace { make<Details::ColorSpaceImpl>(move(description)) };
}

ReadonlyBytes ColorSpace::description() const
{
    return m_color_space->description;
}

}

namespace IPC {

template<>
ErrorOr<void> encode(Encoder& encoder, Gfx::ColorSpace const& color_space)
{
    auto bytes = color_space.m_color_space->description.bytes();
    TRY(encoder.encode<u64>(bytes.size()));
    if (!bytes.is_empty())
        TRY(encoder.append(bytes.data(), bytes.size()));
    return {};
}

template<>
ErrorOr<Gfx::ColorSpace> decode(Decoder& decoder)
{
    // Color space profiles shouldn't be larger than 1 MiB
    static constexpr u64 MAX_COLOR_SPACE_SIZE = 1 * MiB;

    auto size = TRY(decoder.decode<u64>());
    if (size == 0)
        return Gfx::ColorSpace {};

    if (size > MAX_COLOR_SPACE_SIZE)
        return Error::from_string_literal("IPC: Color space size exceeds maximum allowed");

    auto buffer = TRY(ByteBuffer::create_uninitialized(size));
    TRY(decoder.decode_into(buffer.bytes()));
    return Gfx::ColorSpace { make<::Gfx::Details::ColorSpaceImpl>(move(buffer)) };
}

}
