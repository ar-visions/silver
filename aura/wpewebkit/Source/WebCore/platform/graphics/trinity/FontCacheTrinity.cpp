// SPDX-License-Identifier: BSD-2-Clause
// family names and features from skia/FontCacheSkia.cpp (Igalia, BSD-2-Clause)

#include "config.h"
#include "FontCache.h"

#if USE(TRINITY)

#include "Font.h"
#include "FontCustomPlatformData.h"
#include "FontDescription.h"
#include "GraphicsContextTrinity.h"
#include "StyleFontSizeFunctions.h"
#include <hb-ot.h>
#include <wtf/Assertions.h>
#include <wtf/text/CString.h>
#include <wtf/text/CharacterProperties.h>
#include <wtf/unicode/CharacterNames.h>

#if PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
#include "SystemSettings.h"
#endif

namespace WebCore {

void FontCache::platformInit()
{
}

struct TrinityStyle {
    int weight { 400 };
    int width { 5 };
    int slope { 0 };
};

// css weight, stretch and slope as OpenType classes
static TrinityStyle trinityStyle(const FontDescription& fontDescription)
{
    TrinityStyle style;
    auto weight = fontDescription.weight();
    if (weight > FontSelectionValue(0) && weight <= FontSelectionValue(1000))
        style.weight = static_cast<int>(weight);

    auto width = fontDescription.width();
    if (width <= ultraCondensedWidthValue())
        style.width = 1;
    else if (width <= extraCondensedWidthValue())
        style.width = 2;
    else if (width <= condensedWidthValue())
        style.width = 3;
    else if (width <= semiCondensedWidthValue())
        style.width = 4;
    else if (width >= ultraExpandedWidthValue())
        style.width = 9;
    else if (width >= extraExpandedWidthValue())
        style.width = 8;
    else if (width >= expandedWidthValue())
        style.width = 7;
    else if (width >= semiExpandedWidthValue())
        style.width = 6;

    if (auto slope = fontDescription.fontStyleSlope()) {
        if (slope.value() > normalItalicValue() && slope.value() <= italicThreshold())
            style.slope = 1;
        else if (slope.value() > italicThreshold())
            style.slope = 2;
    }
    return style;
}

static std::pair<bool, bool> computeSynthesisProperties(const TrinityTypeface& typeface, bool isColorFont, const FontDescription& fontDescription, OptionSet<FontLookupOptions> synthesisOptions)
{
    if (isColorFont)
        return { false, false };
    bool allowsSyntheticBold = fontDescription.hasAutoFontSynthesisWeight() && !synthesisOptions.contains(FontLookupOptions::DisallowBoldSynthesis);
    bool syntheticBold = allowsSyntheticBold && isFontWeightBold(fontDescription.weight()) && typeface.weight() < 600;
    bool allowsSyntheticOblique = fontDescription.allowsItalicOrObliqueFontSynthesisStyle() && !synthesisOptions.contains(FontLookupOptions::DisallowObliqueSynthesis);
    bool syntheticOblique = allowsSyntheticOblique && isItalic(fontDescription.fontStyleSlope()) && !typeface.slope();
    return { syntheticBold, syntheticOblique };
}

static bool hasColorTables(const TrinityTypeface& typeface)
{
    auto* face = typeface.hbFace();
    return hb_ot_color_has_layers(face) || hb_ot_color_has_png(face) || hb_ot_color_has_svg(face) || hb_ot_color_has_paint(face);
}

RefPtr<Font> FontCache::systemFallbackForCharacterCluster(const FontDescription& description, const Font&, IsForPlatformFont, PreferColoredFont preferColoredFont, StringView stringView)
{
    auto codePoints = stringView.codePoints();
    auto codePointsIterator = codePoints.begin();
    char32_t baseCharacter = *codePointsIterator;
    ++codePointsIterator;
    if (isDefaultIgnorableCodePoint(baseCharacter) || isPrivateUseAreaCharacter(baseCharacter))
        return nullptr;

    bool isEmoji = (codePointsIterator != codePoints.end() && *codePointsIterator == emojiVariationSelector) || preferColoredFont == PreferColoredFont::Yes;
    auto style = trinityStyle(description);
    auto typeface = TrinityTypeface::matchSystem({ }, style.weight, style.width, style.slope, baseCharacter, isEmoji);
    if (!typeface || !typeface->glyphForCharacter(baseCharacter))
        return nullptr;

    auto features = computeFeatures(description, { });
    auto [syntheticBold, syntheticOblique] = computeSynthesisProperties(*typeface, hasColorTables(*typeface), description, { });

    // size-adjust does not apply to fallbacks; font-size-adjust does
    auto size = description.computedSize();
    FontPlatformData alternateFontData(WTF::move(typeface), size, syntheticBold, syntheticOblique, description.orientation(), description.widthVariant(), description.textRenderingMode(), WTF::move(features));
    alternateFontData.updateSizeWithFontSizeAdjust(description.fontSizeAdjust(), size);
    return fontForPlatformData(alternateFontData);
}

Vector<String> FontCache::systemFontFamilies()
{
    trinityNotPorted("listing system font families"_s);
    return { };
}

bool FontCache::isSystemFontForbiddenForEditing(const String&)
{
    return false;
}

Ref<Font> FontCache::lastResortFallbackFont(const FontDescription& fontDescription)
{
    if (RefPtr<Font> font = fontForFamily(fontDescription, "serif"_s))
        return font.releaseNonNull();

    auto style = trinityStyle(fontDescription);
    auto typeface = TrinityTypeface::matchSystem({ }, style.weight, style.width, style.slope);
    RELEASE_ASSERT(typeface);
    auto [syntheticBold, syntheticOblique] = computeSynthesisProperties(*typeface, hasColorTables(*typeface), fontDescription, { });
    FontPlatformData platformData(WTF::move(typeface), fontDescription.computedSize(), syntheticBold, syntheticOblique,
        fontDescription.orientation(), fontDescription.widthVariant(), fontDescription.textRenderingMode(), computeFeatures(fontDescription, { }));
    return fontForPlatformData(platformData);
}

Vector<FontSelectionCapabilities> FontCache::getFontSelectionCapabilitiesInFamily(const AtomString&, AllowUserInstalledFonts)
{
    return { };
}

#if PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
static bool isSystemUIFamilyFont(const String& family)
{
    return family == "-webkit-system-font"_s
        || family == "-webkit-system-ui"_s
        || family == "system-font"_s
        || family == "system-ui"_s
        || family == "ui-sans-serif"_s;
}
#endif

static String getFamilyNameStringFromFamily(const String& family)
{
#if PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
    if (isSystemUIFamilyFont(family))
        return SystemSettings::singleton().defaultSystemFont();
#endif

    // If we're creating a fallback font (e.g. "-webkit-monospace"), convert the name into
    // the fallback name (like "monospace") that fontconfig understands.
    if (family.length() && !family.startsWith("-webkit-"_s))
        return family;

    if (family == *familyNamesData->at(FamilyNamesIndex::StandardFamily) || family == *familyNamesData->at(FamilyNamesIndex::SerifFamily))
        return "serif"_s;
    if (family == *familyNamesData->at(FamilyNamesIndex::SansSerifFamily))
        return "sans-serif"_s;
    if (family == *familyNamesData->at(FamilyNamesIndex::MonospaceFamily))
        return "monospace"_s;
    if (family == *familyNamesData->at(FamilyNamesIndex::CursiveFamily))
        return "cursive"_s;
    if (family == *familyNamesData->at(FamilyNamesIndex::FantasyFamily))
        return "fantasy"_s;
    if (family == *familyNamesData->at(FamilyNamesIndex::MathFamily))
        return "math"_s;

    return emptyString();
}

Vector<hb_feature_t> FontCache::computeFeatures(const FontDescription& fontDescription, const FontCreationContext& fontCreationContext)
{
    FeaturesMap featuresToBeApplied;

    // 7.2. Feature precedence
    // https://www.w3.org/TR/css-fonts-3/#feature-precedence

    // 1. Font features enabled by default, including features required for a given script.

    // FIXME: optical sizing.

    // 2. If the font is defined via an @font-face rule, the font features implied by the
    //    font-feature-settings descriptor in the @font-face rule.
    if (fontCreationContext.fontFaceFeatures()) {
        for (auto& fontFaceFeature : *fontCreationContext.fontFaceFeatures())
            featuresToBeApplied.set(fontFaceFeature.tag(), fontFaceFeature.value());
    }

    // 3. Font features implied by the value of the ‘font-variant’ property, the related ‘font-variant’
    //    subproperties and any other CSS property that uses OpenType features.
    for (auto& newFeature : computeFeatureSettingsFromVariants(fontDescription.variantSettings(), fontCreationContext.fontFeatureValues()))
        featuresToBeApplied.set(newFeature.key, newFeature.value);

    // 4. Feature settings determined by properties other than ‘font-variant’ or ‘font-feature-settings’.
    bool optimizeSpeed = fontDescription.textRenderingMode() == TextRenderingMode::OptimizeSpeed;
    bool shouldDisableLigaturesForSpacing = fontDescription.shouldDisableLigaturesForSpacing();

    // clig and liga are on by default in HarfBuzz.
    auto commonLigatures = fontDescription.variantCommonLigatures();
    if (shouldDisableLigaturesForSpacing || (commonLigatures == FontVariantLigatures::No || (commonLigatures == FontVariantLigatures::Normal && optimizeSpeed))) {
        featuresToBeApplied.set(fontFeatureTag("liga"), 0);
        featuresToBeApplied.set(fontFeatureTag("clig"), 0);
    }

    // dlig is off by default in HarfBuzz.
    auto discretionaryLigatures = fontDescription.variantDiscretionaryLigatures();
    if (!shouldDisableLigaturesForSpacing && discretionaryLigatures == FontVariantLigatures::Yes)
        featuresToBeApplied.set(fontFeatureTag("dlig"), 1);

    // hlig is off by default in HarfBuzz.
    auto historicalLigatures = fontDescription.variantHistoricalLigatures();
    if (!shouldDisableLigaturesForSpacing && historicalLigatures == FontVariantLigatures::Yes)
        featuresToBeApplied.set(fontFeatureTag("hlig"), 1);

    // calt is on by default in HarfBuzz.
    auto contextualAlternates = fontDescription.variantContextualAlternates();
    if (shouldDisableLigaturesForSpacing || (contextualAlternates == FontVariantLigatures::No || (contextualAlternates == FontVariantLigatures::Normal && optimizeSpeed)))
        featuresToBeApplied.set(fontFeatureTag("calt"), 0);

    switch (fontDescription.widthVariant()) {
    case FontWidthVariant::RegularWidth:
        break;
    case FontWidthVariant::HalfWidth:
        featuresToBeApplied.set(fontFeatureTag("hwid"), 1);
        break;
    case FontWidthVariant::ThirdWidth:
        featuresToBeApplied.set(fontFeatureTag("twid"), 1);
        break;
    case FontWidthVariant::QuarterWidth:
        featuresToBeApplied.set(fontFeatureTag("qwid"), 1);
        break;
    }

    switch (fontDescription.variantEastAsianVariant()) {
    case FontVariantEastAsianVariant::Normal:
        break;
    case FontVariantEastAsianVariant::Jis78:
        featuresToBeApplied.set(fontFeatureTag("jp78"), 1);
        break;
    case FontVariantEastAsianVariant::Jis83:
        featuresToBeApplied.set(fontFeatureTag("jp83"), 1);
        break;
    case FontVariantEastAsianVariant::Jis90:
        featuresToBeApplied.set(fontFeatureTag("jp90"), 1);
        break;
    case FontVariantEastAsianVariant::Jis04:
        featuresToBeApplied.set(fontFeatureTag("jp04"), 1);
        break;
    case FontVariantEastAsianVariant::Simplified:
        featuresToBeApplied.set(fontFeatureTag("smpl"), 1);
        break;
    case FontVariantEastAsianVariant::Traditional:
        featuresToBeApplied.set(fontFeatureTag("trad"), 1);
        break;
    }

    switch (fontDescription.variantEastAsianWidth()) {
    case FontVariantEastAsianWidth::Normal:
        break;
    case FontVariantEastAsianWidth::Full:
        featuresToBeApplied.set(fontFeatureTag("fwid"), 1);
        break;
    case FontVariantEastAsianWidth::Proportional:
        featuresToBeApplied.set(fontFeatureTag("pwid"), 1);
        break;
    }

    switch (fontDescription.variantEastAsianRuby()) {
    case FontVariantEastAsianRuby::Normal:
        break;
    case FontVariantEastAsianRuby::Yes:
        featuresToBeApplied.set(fontFeatureTag("ruby"), 1);
        break;
    }

    switch (fontDescription.variantNumericFigure()) {
    case FontVariantNumericFigure::Normal:
        break;
    case FontVariantNumericFigure::LiningNumbers:
        featuresToBeApplied.set(fontFeatureTag("lnum"), 1);
        break;
    case FontVariantNumericFigure::OldStyleNumbers:
        featuresToBeApplied.set(fontFeatureTag("onum"), 1);
        break;
    }

    switch (fontDescription.variantNumericSpacing()) {
    case FontVariantNumericSpacing::Normal:
        break;
    case FontVariantNumericSpacing::ProportionalNumbers:
        featuresToBeApplied.set(fontFeatureTag("pnum"), 1);
        break;
    case FontVariantNumericSpacing::TabularNumbers:
        featuresToBeApplied.set(fontFeatureTag("tnum"), 1);
        break;
    }

    switch (fontDescription.variantNumericFraction()) {
    case FontVariantNumericFraction::Normal:
        break;
    case FontVariantNumericFraction::DiagonalFractions:
        featuresToBeApplied.set(fontFeatureTag("frac"), 1);
        break;
    case FontVariantNumericFraction::StackedFractions:
        featuresToBeApplied.set(fontFeatureTag("afrc"), 1);
        break;
    }

    if (fontDescription.variantNumericOrdinal() == FontVariantNumericOrdinal::Yes)
        featuresToBeApplied.set(fontFeatureTag("ordn"), 1);

    if (fontDescription.variantNumericSlashedZero() == FontVariantNumericSlashedZero::Yes)
        featuresToBeApplied.set(fontFeatureTag("zero"), 1);

    // 5. Font features implied by the value of ‘font-feature-settings’ property.
    for (auto& newFeature : fontDescription.featureSettings())
        featuresToBeApplied.set(newFeature.tag(), newFeature.value());

    if (featuresToBeApplied.isEmpty())
        return { };

    Vector<hb_feature_t> features;
    features.reserveInitialCapacity(featuresToBeApplied.size());
    for (const auto& iter : featuresToBeApplied)
        features.append({ HB_TAG(iter.key[0], iter.key[1], iter.key[2], iter.key[3]), static_cast<uint32_t>(iter.value), 0, static_cast<unsigned>(-1) });
    return features;
}

static bool isUnsupportedFamilyFont(const AtomString& family)
{
    return family == "-apple-system"
        || family == "-apple-system-font";
}

std::unique_ptr<FontPlatformData> FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomString& family, const FontCreationContext& fontCreationContext, OptionSet<FontLookupOptions> options)
{
    if (isUnsupportedFamilyFont(family))
        return nullptr;
    auto familyName = getFamilyNameStringFromFamily(family);
    if (familyName.isEmpty())
        return nullptr;
    auto style = trinityStyle(fontDescription);
    auto typeface = TrinityTypeface::matchSystem(familyName, style.weight, style.width, style.slope);
    if (!typeface)
        return nullptr;

    auto size = fontDescription.adjustedSizeForFontFace(fontCreationContext.sizeAdjust());
    auto features = computeFeatures(fontDescription, fontCreationContext);
    auto [syntheticBold, syntheticOblique] = computeSynthesisProperties(*typeface, hasColorTables(*typeface), fontDescription, options);
    FontPlatformData platformData(WTF::move(typeface), size, syntheticBold, syntheticOblique, fontDescription.orientation(), fontDescription.widthVariant(), fontDescription.textRenderingMode(), WTF::move(features));

    platformData.updateSizeWithFontSizeAdjust(fontDescription.fontSizeAdjust(), fontDescription.computedSize());
    return makeUnique<FontPlatformData>(platformData);
}

ASCIILiteral FontCache::platformAlternateFamilyName(const String&)
{
    return { };
}

void FontCache::platformInvalidate()
{
}

void FontCache::platformPurgeInactiveFontData()
{
}

} // namespace WebCore

#endif // USE(TRINITY)
