// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "GlyphPage.h"

#if USE(TRINITY)

#include "Font.h"
#include "FontCascade.h"

namespace WebCore {

bool GlyphPage::fill(std::span<const char16_t> buffer)
{
    const Font& font = this->font();
    auto* hbFont = font.platformData().hbFont();
    if (!hbFont)
        return false;

    StringView stringView(buffer);
    auto codePoints = stringView.codePoints();
    auto codePointsIterator = codePoints.begin();

    bool haveGlyphs = false;
    for (unsigned i = 0; i < GlyphPage::size && codePointsIterator != codePoints.end(); ++i, ++codePointsIterator) {
        char32_t character = *codePointsIterator;
        hb_codepoint_t glyph;
        if (hb_font_get_nominal_glyph(hbFont, character, &glyph)) {
            setGlyphForIndex(i, glyph, font.colorGlyphType(glyph));
            haveGlyphs = true;
        }
    }
    return haveGlyphs;
}

} // namespace WebCore

#endif // USE(TRINITY)
