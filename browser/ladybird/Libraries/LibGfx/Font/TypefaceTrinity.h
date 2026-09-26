/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/FlyString.h>
#include <LibGfx/Font/Typeface.h>

namespace Gfx {

enum class SystemUIFontKind : u8 {
    System,
    Serif,
    Monospace,
    Rounded,
};

struct SystemUIFontStyle {
    SystemUIFontKind kind;
    u16 weight;
    u16 width;
    u8 slope;
};

// a trinity CanvasFont, held by its webgfx id
class TypefaceTrinity : public Gfx::Typeface {
    AK_MAKE_NONCOPYABLE(TypefaceTrinity);

public:
    static ErrorOr<NonnullRefPtr<TypefaceTrinity>> load_from_buffer(ReadonlyBytes, u32 ttc_index = 0, RefPtr<FontDataBacking> = {});
    static ErrorOr<RefPtr<TypefaceTrinity>> match_system_ui(SystemUIFontKind, float point_size, u16 weight, u16 width, u8 slope);
    static ErrorOr<RefPtr<TypefaceTrinity>> match_family_style(StringView family_name, u16 weight, u16 width, u8 slope);
    static ErrorOr<RefPtr<TypefaceTrinity>> find_typeface_for_code_point(u32 code_point, u16 weight, u16 width, u8 slope, bool prefer_color_emoji);
    static Optional<FlyString> resolve_generic_family(StringView family_name, u16 weight, u8 slope);

    virtual ~TypefaceTrinity() override;

    RefPtr<TypefaceTrinity const> clone_with_variations(Vector<FontVariationAxis> const& axes) const;

    virtual u32 glyph_count() const override;
    virtual u16 units_per_em() const override;
    virtual u32 glyph_id_for_code_point(u32 code_point) const override;
    virtual FlyString const& family() const override;
    virtual u16 weight() const override;
    virtual u16 width() const override;
    virtual u8 slope() const override;

    virtual ReadonlyBytes buffer() const LIFETIME_BOUND override { return m_buffer; }
    virtual u32 ttc_index() const override { return m_ttc_index; }

    int webgfx_font() const { return m_font; }

private:
    TypefaceTrinity(int font, ReadonlyBytes, u32 ttc_index);

    virtual bool is_trinity() const override { return true; }

    int m_font { 0 };
    ReadonlyBytes m_buffer;
    u32 m_ttc_index { 0 };
    int m_style[3] { 400, 5, 0 };
    mutable Optional<FlyString> m_family;
};

template<>
inline bool Typeface::fast_is<TypefaceTrinity>() const { return is_trinity(); }

}

namespace IPC {

template<>
ErrorOr<void> encode(Encoder&, Gfx::SystemUIFontStyle const&);

template<>
ErrorOr<Gfx::SystemUIFontStyle> decode(Decoder&);

}
