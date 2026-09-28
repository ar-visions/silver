// SPDX-License-Identifier: BSD-2-Clause
// from skia/FontCascadeSkia.cpp (Igalia, BSD-2-Clause)

#include "config.h"
#include "FontCascadeInlines.h"

#if USE(TRINITY)
#include "FontCache.h"
#include "GraphicsContextTrinity.h"
#include "SurrogatePairAwareTextIterator.h"
#include <wtf/text/CharacterProperties.h>

namespace WebCore {

void FontCascade::drawGlyphs(GraphicsContext& graphicsContext, const Font& font, std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances, const FloatPoint& position, FontSmoothingMode)
{
    auto& platformData = font.platformData();
    if (!platformData.size() || glyphs.empty() || !platformData.typeface())
        return;
    if (platformData.orientation() == FontOrientation::Vertical)
        trinityNotPorted("vertical text"_s);
    if (platformData.syntheticBold() || platformData.syntheticOblique())
        trinityNotPorted("synthetic bold and oblique text"_s);
    Vector<uint32_t, 256> ids;
    Vector<float, 256> xs;
    Vector<float, 256> ys;
    FloatPoint pen = position;
    for (size_t i = 0; i < glyphs.size(); ++i) {
        // deleted glyphs take space but draw nothing
        if (glyphs[i] != deletedGlyph) {
            ids.append(glyphs[i]);
            xs.append(pen.x());
            ys.append(pen.y());
        }
        pen.move(advances[i].width(), advances[i].height());
    }
    if (ids.isEmpty())
        return;
    static_cast<GraphicsContextTrinity&>(graphicsContext).drawTrinityGlyphs(platformData.typeface()->webgfxFont(), platformData.size(), ids.span(), xs.span(), ys.span());
}

bool FontCascade::canUseGlyphDisplayList(const Style::ComputedStyle&)
{
    return true;
}

ResolvedEmojiPolicy FontCascade::resolveEmojiPolicy(FontVariantEmoji fontVariantEmoji, char32_t character)
{
    switch (fontVariantEmoji) {
    case FontVariantEmoji::Normal:
        if (isEmojiWithPresentationByDefault(character)
            || isEmojiModifierBase(character)
            || isEmojiFitzpatrickModifier(character))
            return ResolvedEmojiPolicy::RequireEmoji;
        break;
    case FontVariantEmoji::Unicode:
        if (u_hasBinaryProperty(character, UCHAR_EMOJI))
            return isEmojiWithPresentationByDefault(character) ? ResolvedEmojiPolicy::RequireEmoji : ResolvedEmojiPolicy::RequireText;
        break;
    case FontVariantEmoji::Text:
        return ResolvedEmojiPolicy::RequireText;
    case FontVariantEmoji::Emoji:
        if (u_hasBinaryProperty(character, UCHAR_EMOJI))
            return ResolvedEmojiPolicy::RequireEmoji;
    }

    return ResolvedEmojiPolicy::NoPreference;
}

RefPtr<const Font> FontCascade::fontForCombiningCharacterSequence(StringView stringView) const
{
    ASSERT(!stringView.isEmpty());
    auto codePoints = stringView.codePoints();
    auto codePointsIterator = codePoints.begin();
    char32_t baseCharacter = *codePointsIterator;
    ++codePointsIterator;
    bool isOnlySingleCodePoint = codePointsIterator == codePoints.end();

    auto [emojiPolicy, shouldForceEmojiFont] = [&]() -> std::pair<ResolvedEmojiPolicy, bool> {
        if (!isOnlySingleCodePoint) {
            if (*codePointsIterator == emojiVariationSelector)
                return { ResolvedEmojiPolicy::RequireEmoji, true };

            if (*codePointsIterator == textVariationSelector)
                return { ResolvedEmojiPolicy::RequireText, false };
        }

        auto emojiPolicy = resolveEmojiPolicy(m_fontDescription.variantEmoji(), baseCharacter);
        return { emojiPolicy, emojiPolicy == ResolvedEmojiPolicy::RequireEmoji && m_fontDescription.variantEmoji() == FontVariantEmoji::Emoji };
    }();

    char32_t baseCharacterForBaseFont = baseCharacter;
    if (shouldForceEmojiFont) {
        // System fallback doesn't support character sequences, so here we override
        // the base character with the cat emoji to try to force an emoji font.
        baseCharacterForBaseFont = emojiCat;
    }
    GlyphData baseCharacterGlyphData = glyphDataForCharacter(baseCharacterForBaseFont, false, FontVariant::Normal, emojiPolicy);
    if (!baseCharacterGlyphData.glyph)
        return nullptr;

    auto fontMatchesEmojiPolicy = [](const Font* font, ResolvedEmojiPolicy emojiPolicy) -> bool {
        if (!font)
            return false;

        switch (emojiPolicy) {
        case ResolvedEmojiPolicy::RequireEmoji:
            return font->platformData().isColorBitmapFont();
        case ResolvedEmojiPolicy::RequireText:
            return !font->platformData().isColorBitmapFont();
        case ResolvedEmojiPolicy::NoPreference:
            break;
        }
        return true;
    };

    if (isOnlySingleCodePoint && !shouldForceEmojiFont && fontMatchesEmojiPolicy(baseCharacterGlyphData.font.get(), emojiPolicy))
        return baseCharacterGlyphData.font.get();

    bool triedBaseCharacterFont = false;
    for (unsigned i = 0; !fallbackRangesAt(i).isNull(); ++i) {
        auto& fontRanges = fallbackRangesAt(i);
        if (fontRanges.isGenericFontFamily() && isPrivateUseAreaCharacter(baseCharacter))
            continue;

        const Font* font = fontRanges.fontForCharacter(baseCharacter);
        if (!font)
            continue;

        if (!fontMatchesEmojiPolicy(font, emojiPolicy))
            continue;

        if (font == baseCharacterGlyphData.font)
            triedBaseCharacterFont = true;

        if (font->canRenderCombiningCharacterSequence(stringView))
            return font;
    }

    if (!triedBaseCharacterFont && baseCharacterGlyphData.font && baseCharacterGlyphData.font->canRenderCombiningCharacterSequence(stringView))
        return baseCharacterGlyphData.font.get();

    bool clusterContainsOtherNonDefaultIgnorableCodePoints = [&] -> bool {
        if (isOnlySingleCodePoint)
            return false;

        do {
            if (!isDefaultIgnorableCodePoint(*codePointsIterator))
                return true;
            ++codePointsIterator;
        } while (codePointsIterator != codePoints.end());

        return false;
    }();

    // Try a system fallback for the whole cluster if needed.
    if (clusterContainsOtherNonDefaultIgnorableCodePoints) {
        auto preferColoredFont = emojiPolicy == ResolvedEmojiPolicy::RequireEmoji ? FontCache::PreferColoredFont::Yes : FontCache::PreferColoredFont::No;
        if (auto systemFallback = FontCache::forCurrentThread().systemFallbackForCharacterCluster(m_fontDescription, fallbackRangesAt(0).fontForFirstRange(), IsForPlatformFont::No, preferColoredFont, stringView)) {
            if (systemFallback->canRenderCombiningCharacterSequence(stringView))
                return systemFallback.get();
        }
    }

    return baseCharacterGlyphData.font.get();
}

} // namespace WebCore

#endif // USE(TRINITY)
