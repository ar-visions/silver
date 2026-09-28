// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "FontCustomPlatformData.h"

#if USE(TRINITY)

#include "FontCache.h"
#include "FontCreationContext.h"
#include "FontDescription.h"
#include "FontVariationsTrinity.h"
#include "NotImplemented.h"
#include "SharedBuffer.h"

namespace WebCore {

FontCustomPlatformData::FontCustomPlatformData(Ref<TrinityTypeface>&& typeface, FontPlatformData::CreationData&& data)
    : m_typeface(WTF::move(typeface))
    , creationData(WTF::move(data))
    , m_renderingResourceIdentifier(RenderingResourceIdentifier::generate())
{
}

FontCustomPlatformData::~FontCustomPlatformData() = default;

static uint32_t tagValue(const FontTag& tag)
{
    return (static_cast<uint8_t>(tag[0]) << 24) | (static_cast<uint8_t>(tag[1]) << 16) | (static_cast<uint8_t>(tag[2]) << 8) | static_cast<uint8_t>(tag[3]);
}

FontPlatformData FontCustomPlatformData::fontPlatformData(const FontDescription& description, const FontCreationContext& fontCreationContext)
{
    Ref typeface = m_typeface;

    auto defaultValues = defaultFontVariationValues(typeface);
    bool hasWeightVariationAxis = defaultValues.contains(FontVariationAxisTag::wght);
    bool hasSlopeVariationAxis = defaultValues.contains(FontVariationAxisTag::slnt) || defaultValues.contains(FontVariationAxisTag::ital);
    if (!defaultValues.isEmpty()) {
        Vector<TrinityTypeface::Axis> variationsToBeApplied;
        auto applyVariation = [&](const FontTag& tag, float value) {
            auto iterator = defaultValues.find(tag);
            if (iterator == defaultValues.end())
                return;
            variationsToBeApplied.append({ tagValue(tag), iterator->value.clamp(value) });
        };

        float weight = description.weight();
        if (auto weightValue = fontCreationContext.fontFaceCapabilities().weight)
            weight = std::max(std::min(weight, static_cast<float>(weightValue->maximum)), static_cast<float>(weightValue->minimum));
        applyVariation(FontVariationAxisTag::wght, weight);

        float width = description.width();
        if (auto widthValue = fontCreationContext.fontFaceCapabilities().width)
            width = std::max(std::min(width, static_cast<float>(widthValue->maximum)), static_cast<float>(widthValue->minimum));
        applyVariation(FontVariationAxisTag::wdth, width);

        if (variationStyleAxis(description, fontCreationContext.fontFaceCapabilities()) == FontStyleAxis::ital)
            applyVariation(FontVariationAxisTag::ital, 1);
        else {
            float slope = description.fontStyleSlope().value_or(normalItalicValue());
            if (auto slopeValue = fontCreationContext.fontFaceCapabilities().slope)
                slope = std::max(std::min(slope, static_cast<float>(slopeValue->maximum)), static_cast<float>(slopeValue->minimum));
            // slnt is counter-clockwise; CSS oblique is clockwise
            applyVariation(FontVariationAxisTag::slnt, -slope);
        }

        for (auto& variation : description.variationSettings())
            applyVariation(variation.tag(), variation.value());

        if (!variationsToBeApplied.isEmpty())
            typeface = retrieveOrAddCachedTypeface(WTF::move(variationsToBeApplied));
    }

    auto size = description.adjustedSizeForFontFace(fontCreationContext.sizeAdjust());
    auto features = FontCache::computeFeatures(description, fontCreationContext);

    FontPlatformData platformData(typeface.ptr(), size, computeSyntheticBold(hasWeightVariationAxis, description, fontCreationContext), computeSyntheticItalic(hasSlopeVariationAxis, description, fontCreationContext), description.orientation(), description.widthVariant(), description.textRenderingMode(), WTF::move(features), this);
    platformData.updateSizeWithFontSizeAdjust(description.fontSizeAdjust(), description.computedSize());
    return platformData;
}

RefPtr<FontCustomPlatformData> FontCustomPlatformData::create(SharedBuffer& buffer, const String& itemInCollection)
{
    RefPtr<TrinityTypeface> typeface;
    bool isCollection = spanHasPrefix(buffer.span(), "ttcf"_span);
    if (itemInCollection.isNull() || !isCollection)
        typeface = TrinityTypeface::create(Vector<uint8_t> { buffer.span() });
    else {
        for (unsigned index = 0; ; ++index) {
            typeface = TrinityTypeface::create(Vector<uint8_t> { buffer.span() }, index);
            if (!typeface || equalIgnoringASCIICase(itemInCollection, typeface->familyName()))
                break;
        }
    }
    if (!typeface)
        return nullptr;

    FontPlatformData::CreationData creationData = { buffer, itemInCollection };
    return adoptRef(new FontCustomPlatformData(typeface.releaseNonNull(), WTF::move(creationData)));
}

RefPtr<FontCustomPlatformData> FontCustomPlatformData::createMemorySafe(SharedBuffer&, const String&)
{
    return nullptr;
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalLettersIgnoringASCIICase(format, "truetype"_s)
        || equalLettersIgnoringASCIICase(format, "opentype"_s)
#if HAVE(WOFF_SUPPORT) || USE(WOFF2)
        || equalLettersIgnoringASCIICase(format, "woff2"_s)
#if ENABLE(VARIATION_FONTS)
        || equalLettersIgnoringASCIICase(format, "woff2-variations"_s)
#endif
#endif
#if ENABLE(VARIATION_FONTS)
        || equalLettersIgnoringASCIICase(format, "woff-variations"_s)
        || equalLettersIgnoringASCIICase(format, "truetype-variations"_s)
        || equalLettersIgnoringASCIICase(format, "opentype-variations"_s)
#endif
        || equalLettersIgnoringASCIICase(format, "woff"_s)
        || equalLettersIgnoringASCIICase(format, "svg"_s);
}

bool FontCustomPlatformData::supportsTechnology(const FontTechnology&)
{
    notImplemented();
    return true;
}

std::optional<Ref<FontCustomPlatformData>> FontCustomPlatformData::tryMakeFromSerializationData(FontCustomPlatformSerializedData&& data, bool)
{
    RefPtr fontCustomPlatformData = FontCustomPlatformData::create(WTF::move(data.fontFaceData), data.itemInCollection);
    if (!fontCustomPlatformData)
        return std::nullopt;
    fontCustomPlatformData->m_renderingResourceIdentifier = data.renderingResourceIdentifier;
    return fontCustomPlatformData.releaseNonNull();
}

FontCustomPlatformSerializedData FontCustomPlatformData::serializedData() const
{
    return FontCustomPlatformSerializedData { creationData.fontFaceData, creationData.itemInCollection, m_renderingResourceIdentifier };
}

Ref<TrinityTypeface> FontCustomPlatformData::retrieveOrAddCachedTypeface(Vector<TrinityTypeface::Axis>&& axes)
{
    WTF::Hasher hasher;
    for (auto& axis : axes) {
        WTF::add(hasher, axis.tag);
        WTF::add(hasher, axis.value);
    }
    constexpr size_t cacheMaximumSize = 8;
    if (m_variationTypefacesCache.size() >= cacheMaximumSize)
        m_variationTypefacesCache.remove(m_variationTypefacesCache.random());
    auto addResult = m_variationTypefacesCache.ensure(hasher.hash(), [&]() -> Ref<TrinityTypeface> {
        if (RefPtr variation = m_typeface->withAxes(WTF::move(axes)))
            return variation.releaseNonNull();
        return m_typeface;
    });
    return addResult.iterator->value;
}

void FontCustomPlatformData::clearUnusedVariationTypefacesCacheEntries() const
{
    m_variationTypefacesCache.removeIf([](const auto& entry) {
        return entry.value->hasOneRef();
    });
}

} // namespace WebCore

#endif // USE(TRINITY)
