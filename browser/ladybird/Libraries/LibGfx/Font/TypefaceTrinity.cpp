/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteString.h>
#include <LibCore/AnonymousBuffer.h>
#include <LibCore/MappedFile.h>
#include <LibGfx/Font/TypefaceTrinity.h>
#include <LibGfx/WebGfx.h>
#include <LibIPC/Decoder.h>
#include <LibIPC/Encoder.h>

namespace Gfx {

static constexpr int path_capacity = 4096;
static constexpr int family_capacity = 256;

ErrorOr<NonnullRefPtr<TypefaceTrinity>> TypefaceTrinity::load_from_buffer(AK::ReadonlyBytes buffer, u32 ttc_index, RefPtr<FontDataBacking> backing)
{
    // FreeType reads the bytes in place: with no backing, the
    // caller keeps them alive (a resource attaches one after)
    auto font = webgfx_font_open(buffer.data(), static_cast<i64>(buffer.size()), static_cast<int>(ttc_index));
    if (font == 0)
        return Error::from_string_literal("Failed to load typeface from buffer");

    auto typeface = adopt_ref(*new TypefaceTrinity { font, buffer, ttc_index });
    if (backing)
        typeface->set_font_data(backing.release_nonnull());
    return typeface;
}

static ErrorOr<RefPtr<TypefaceTrinity>> load_system_font(char const* family, u16 weight, u16 width, u8 slope, u32 code_point, bool emoji)
{
    u8 file[path_capacity];
    u8 family_out[family_capacity];
    int index = 0;
    if (!webgfx_font_match(family, weight, width, slope, code_point, emoji, file, path_capacity, family_out, family_capacity, &index))
        return RefPtr<TypefaceTrinity> {};
    auto mapped = TRY(Core::MappedFile::map(StringView { reinterpret_cast<char const*>(file), __builtin_strlen(reinterpret_cast<char const*>(file)) }));
    auto typeface = TRY(Typeface::try_load_from_mapped_file(move(mapped), static_cast<u32>(index)));
    return static_ptr_cast<TypefaceTrinity>(typeface);
}

ErrorOr<RefPtr<TypefaceTrinity>> TypefaceTrinity::match_system_ui(SystemUIFontKind, float, u16, u16, u8)
{
    // system ui fonts are a macOS CoreText feature
    return RefPtr<TypefaceTrinity> {};
}

ErrorOr<RefPtr<TypefaceTrinity>> TypefaceTrinity::match_family_style(StringView family_name, u16 weight, u16 width, u8 slope)
{
    return load_system_font(ByteString(family_name).characters(), weight, width, slope, 0, false);
}

ErrorOr<RefPtr<TypefaceTrinity>> TypefaceTrinity::find_typeface_for_code_point(u32 code_point, u16 weight, u16 width, u8 slope, bool prefer_color_emoji)
{
    return load_system_font(nullptr, weight, width, slope, code_point, prefer_color_emoji);
}

Optional<FlyString> TypefaceTrinity::resolve_generic_family(StringView family_name, u16 weight, u8 slope)
{
    u8 file[path_capacity];
    u8 family_out[family_capacity];
    int index = 0;
    if (!webgfx_font_match(ByteString(family_name).characters(), weight, 5, slope, 0, false, file, path_capacity, family_out, family_capacity, &index))
        return {};
    auto result = FlyString::from_utf8({ reinterpret_cast<char const*>(family_out), __builtin_strlen(reinterpret_cast<char const*>(family_out)) });
    if (result.is_error())
        return {};
    return result.release_value();
}

RefPtr<TypefaceTrinity const> TypefaceTrinity::clone_with_variations(Vector<FontVariationAxis> const& axes) const
{
    if (axes.is_empty())
        return this;

    auto font = webgfx_font_open(m_buffer.data(), static_cast<i64>(m_buffer.size()), static_cast<int>(m_ttc_index));
    if (font == 0)
        return {};

    Vector<u32> tags;
    Vector<float> values;
    for (auto const& axis : axes) {
        tags.append(axis.tag.to_u32());
        values.append(axis.value);
    }
    webgfx_font_set_axes(font, tags.data(), values.data(), static_cast<int>(tags.size()));

    auto typeface = adopt_ref(*new TypefaceTrinity { font, m_buffer, m_ttc_index });
    typeface->copy_font_data_from(*this);
    return typeface;
}

TypefaceTrinity::TypefaceTrinity(int font, ReadonlyBytes buffer, u32 ttc_index)
    : m_font(font)
    , m_buffer(buffer)
    , m_ttc_index(ttc_index)
{
    webgfx_font_style(m_font, m_style);
}

TypefaceTrinity::~TypefaceTrinity()
{
    webgfx_font_close(m_font);
}

u32 TypefaceTrinity::glyph_count() const
{
    return static_cast<u32>(webgfx_font_glyph_count(m_font));
}

u16 TypefaceTrinity::units_per_em() const
{
    return static_cast<u16>(webgfx_font_units_per_em(m_font));
}

u32 TypefaceTrinity::glyph_id_for_code_point(u32 code_point) const
{
    return webgfx_font_glyph_index(m_font, code_point);
}

FlyString const& TypefaceTrinity::family() const
{
    return m_family.ensure([&] {
        u8 name[family_capacity];
        auto length = webgfx_font_family(m_font, name, family_capacity);
        if (length < 0)
            return FlyString {};
        return FlyString::from_utf8_without_validation(ReadonlyBytes { name, static_cast<size_t>(length) });
    });
}

u16 TypefaceTrinity::weight() const
{
    return static_cast<u16>(m_style[0]);
}

u16 TypefaceTrinity::width() const
{
    return static_cast<u16>(m_style[1]);
}

u8 TypefaceTrinity::slope() const
{
    return static_cast<u8>(m_style[2]);
}

}

namespace IPC {

template<>
ErrorOr<void> encode(Encoder& encoder, Gfx::SystemUIFontStyle const& style)
{
    TRY(encoder.encode(style.kind));
    TRY(encoder.encode(style.weight));
    TRY(encoder.encode(style.width));
    TRY(encoder.encode(style.slope));
    return {};
}

template<>
ErrorOr<Gfx::SystemUIFontStyle> decode(Decoder& decoder)
{
    auto kind = TRY(decoder.decode<Gfx::SystemUIFontKind>());
    auto weight = TRY(decoder.decode<u16>());
    auto width = TRY(decoder.decode<u16>());
    auto slope = TRY(decoder.decode<u8>());
    return Gfx::SystemUIFontStyle { kind, weight, width, slope };
}

}
