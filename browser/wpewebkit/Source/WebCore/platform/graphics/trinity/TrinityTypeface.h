// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#if USE(TRINITY)

#include <hb.h>
#include <span>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// a font file's bytes, drawn by webgfx and shaped by HarfBuzz
class TrinityTypeface : public ThreadSafeRefCounted<TrinityTypeface> {
public:
    struct Axis {
        uint32_t tag { 0 };
        float value { 0 };
    };

    // null when the bytes are not a font
    WEBCORE_EXPORT static RefPtr<TrinityTypeface> create(Vector<uint8_t>&&, unsigned index = 0, Vector<Axis>&& = { });
    WEBCORE_EXPORT static RefPtr<TrinityTypeface> createFromFile(const String& path, unsigned index = 0);
    // a system font by fontconfig; code point 0 matches any
    WEBCORE_EXPORT static RefPtr<TrinityTypeface> matchSystem(const String& family, int weight, int width, int slope, char32_t = 0, bool emoji = false);
    WEBCORE_EXPORT ~TrinityTypeface();

    // the same bytes with these variation axes set
    RefPtr<TrinityTypeface> withAxes(Vector<Axis>&&) const;

    int webgfxFont() const { return m_font; }
    hb_face_t* hbFace() const { return m_hbFace; }
    std::span<const uint8_t> bytes() const LIFETIME_BOUND { return m_bytes.span(); }
    unsigned index() const { return m_index; }
    const Vector<Axis>& axes() const LIFETIME_BOUND { return m_axes; }
    const String& familyName() const LIFETIME_BOUND { return m_familyName; }
    // OpenType weight (100-900), width (1-9), slope (0 upright)
    int weight() const { return m_style[0]; }
    int width() const { return m_style[1]; }
    int slope() const { return m_style[2]; }
    unsigned unitsPerEm() const;
    unsigned glyphCount() const;
    uint32_t glyphForCharacter(char32_t) const;

private:
    TrinityTypeface(Vector<uint8_t>&&, unsigned index, int font, hb_face_t*, Vector<Axis>&&);

    Vector<uint8_t> m_bytes;
    unsigned m_index { 0 };
    int m_font { 0 };
    hb_face_t* m_hbFace { nullptr };
    Vector<Axis> m_axes;
    String m_familyName;
    int m_style[3] { 400, 5, 0 };
};

// a shared HarfBuzz font: font data copies share one
class TrinityHbFont : public ThreadSafeRefCounted<TrinityHbFont> {
public:
    static Ref<TrinityHbFont> create(hb_font_t* font) { return adoptRef(*new TrinityHbFont(font)); }
    ~TrinityHbFont() { hb_font_destroy(m_font); }
    hb_font_t* font() const { return m_font; }

private:
    explicit TrinityHbFont(hb_font_t* font)
        : m_font(font)
    {
    }
    hb_font_t* m_font;
};

} // namespace WebCore

#endif // USE(TRINITY)
