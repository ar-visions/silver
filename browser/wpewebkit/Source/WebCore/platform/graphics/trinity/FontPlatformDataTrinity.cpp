// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "FontPlatformData.h"

#if USE(TRINITY)

#include "FontCascade.h"
#include "FontCustomPlatformData.h"
#include "FontVariationsTrinity.h"
#include "SharedBuffer.h"
#include <hb-ot.h>
#include <wtf/Hasher.h>

namespace WebCore {

FontPlatformData::FontPlatformData(RefPtr<TrinityTypeface>&& typeface, float size, bool syntheticBold, bool syntheticOblique, FontOrientation orientation, FontWidthVariant widthVariant, TextRenderingMode textRenderingMode, Vector<hb_feature_t>&& features, const FontCustomPlatformData* customPlatformData)
    : FontPlatformData(WTF::move(typeface), FontMetadata { size, orientation, widthVariant, textRenderingMode, syntheticBold, syntheticOblique }, WTF::move(features), customPlatformData)
{
}

FontPlatformData::FontPlatformData(RefPtr<TrinityTypeface>&& typeface, const FontMetadata& metadata, Vector<hb_feature_t>&& features, const FontCustomPlatformData* customPlatformData)
    : FontPlatformData(metadata, customPlatformData)
{
    m_typeface = WTF::move(typeface);
    m_features = WTF::move(features);
    platformDataInit();
}

FontPlatformData::FontPlatformData(const FontMetadata& metadata, RefPtr<FontCustomPlatformData>&& customPlatformData)
    : FontPlatformData(metadata, customPlatformData.get())
{
    m_typeface = customPlatformData->m_typeface.ptr();
    platformDataInit();
}

FontPlatformData::~FontPlatformData()
{
    if (m_customPlatformData) {
        m_typeface = nullptr;
        m_customPlatformData->clearUnusedVariationTypefacesCacheEntries();
    }
}

static bool hasColorTables(hb_face_t* face)
{
    return hb_ot_color_has_layers(face) || hb_ot_color_has_png(face) || hb_ot_color_has_svg(face) || hb_ot_color_has_paint(face);
}

// WebKit's whitespace rules before the font's own cmap
static bool mappedGlyph(hb_font_t* font, hb_codepoint_t unicode, hb_codepoint_t variation, hb_codepoint_t* glyph, bool isColor)
{
    if (FontCascade::treatAsSpace(unicode))
        unicode = space;
    else if (FontCascade::treatAsZeroWidthSpaceInComplexScript(unicode))
        unicode = zeroWidthSpace;
    auto* parent = hb_font_get_parent(font);
    if (hb_font_get_glyph(parent, unicode, variation, glyph))
        return true;
    if (!variation)
        return false;
    // a selector the font cannot honor: emoji needs color
    if (variation == emojiVariationSelector && !isColor)
        return false;
    if (variation == textVariationSelector && isColor)
        return false;
    return hb_font_get_glyph(parent, unicode, 0, glyph);
}

static hb_font_funcs_t* glyphMappingFunctions()
{
    static hb_font_funcs_t* functions = [] {
        auto* functions = hb_font_funcs_create();
        hb_font_funcs_set_nominal_glyph_func(functions, [](hb_font_t* font, void* isColor, hb_codepoint_t unicode, hb_codepoint_t* glyph, void*) -> hb_bool_t {
            return mappedGlyph(font, unicode, 0, glyph, !!isColor);
        }, nullptr, nullptr);
        hb_font_funcs_set_variation_glyph_func(functions, [](hb_font_t* font, void* isColor, hb_codepoint_t unicode, hb_codepoint_t variation, hb_codepoint_t* glyph, void*) -> hb_bool_t {
            return mappedGlyph(font, unicode, variation, glyph, !!isColor);
        }, nullptr, nullptr);
        hb_font_funcs_make_immutable(functions);
        return functions;
    }();
    return functions;
}

// a HarfBuzz font at this size, positions in 16.16 pixels
void FontPlatformData::platformDataInit()
{
    if (!m_typeface)
        return;
    m_isColorBitmapFont = hasColorTables(m_typeface->hbFace());
    auto* parent = hb_font_create(m_typeface->hbFace());
    int scale = clampTo<int>(m_metadata.pointSize * (1 << 16));
    hb_font_set_scale(parent, scale, scale);
    hb_font_set_ptem(parent, m_metadata.pointSize);
    auto& axes = m_typeface->axes();
    if (!axes.isEmpty()) {
        Vector<hb_variation_t> variations;
        for (auto& axis : axes)
            variations.append({ axis.tag, axis.value });
        hb_font_set_variations(parent, variations.span().data(), variations.size());
    }
    // the sub font takes the rest (advances, extents) from its parent
    auto* font = hb_font_create_sub_font(parent);
    hb_font_destroy(parent);
    hb_font_set_funcs(font, glyphMappingFunctions(), m_isColorBitmapFont ? reinterpret_cast<void*>(1) : nullptr, nullptr);
    if (m_metadata.isSyntheticOblique)
        hb_font_set_synthetic_slant(font, 0.25f);
    hb_font_make_immutable(font);
    m_hbFont = TrinityHbFont::create(font);
}

std::optional<FontPlatformData> FontPlatformData::fromIPCData(const FontMetadata& metadata, IPCData&& ipcData)
{
    return WTF::switchOn(ipcData,
        [&] (FontPlatformSerializedData& d) -> std::optional<FontPlatformData> {
            Vector<TrinityTypeface::Axis> axes;
            for (size_t i = 0; i < d.axisTags.size() && i < d.axisValues.size(); ++i)
                axes.append({ d.axisTags[i], d.axisValues[i] });
            if (RefPtr typeface = TrinityTypeface::create(WTF::move(d.typefaceData), d.index, WTF::move(axes)))
                return FontPlatformData(WTF::move(typeface), metadata, { });
            return std::nullopt;
        },
        [&] (CustomFontCreationData& d) -> std::optional<FontPlatformData> {
            auto fontFaceData = SharedBuffer::create(WTF::move(d.fontFaceData));
            if (RefPtr fontCustomPlatformData = FontCustomPlatformData::create(fontFaceData, d.itemInCollection))
                return FontPlatformData(metadata, WTF::move(fontCustomPlatformData));
            return std::nullopt;
        }
    );
}

FontPlatformData::IPCData FontPlatformData::toIPCData() const
{
    if (auto* data = creationData())
        return CustomFontCreationData { { data->fontFaceData->span() }, data->itemInCollection };
    FontPlatformSerializedData data;
    data.typefaceData = m_typeface->bytes();
    data.index = m_typeface->index();
    for (auto& axis : m_typeface->axes()) {
        data.axisTags.append(axis.tag);
        data.axisValues.append(axis.value);
    }
    return data;
}

bool FontPlatformData::isFixedPitch() const
{
    auto* face = m_typeface ? m_typeface->hbFace() : nullptr;
    if (!face)
        return false;
    // post table: isFixedPitch, a 32-bit field at byte 12
    auto* blob = hb_face_reference_table(face, HB_TAG('p', 'o', 's', 't'));
    unsigned length = 0;
    auto* data = reinterpret_cast<const uint8_t*>(hb_blob_get_data(blob, &length));
    bool fixed = length >= 16 && (data[12] | data[13] | data[14] | data[15]);
    hb_blob_destroy(blob);
    return fixed;
}

unsigned FontPlatformData::hash() const
{
    return computeHash(m_typeface.get(), m_isHashTableDeletedValue, m_metadata.widthVariant, m_metadata.textRenderingMode, m_metadata.orientation, m_metadata.isSyntheticBold, m_metadata.isSyntheticOblique);
}

bool FontPlatformData::platformIsEqual(const FontPlatformData& other) const
{
    return m_typeface == other.m_typeface && m_features == other.m_features;
}

#if !LOG_DISABLED
String FontPlatformData::description() const
{
    return String();
}
#endif

String FontPlatformData::familyName() const
{
    return m_typeface ? m_typeface->familyName() : String();
}

RefPtr<SharedBuffer> FontPlatformData::openTypeTable(uint32_t table) const
{
    if (!m_typeface)
        return nullptr;
    auto* blob = hb_face_reference_table(m_typeface->hbFace(), table);
    unsigned length = 0;
    auto* data = hb_blob_get_data(blob, &length);
    RefPtr<SharedBuffer> buffer;
    if (length)
        buffer = SharedBuffer::create(std::span { reinterpret_cast<const uint8_t*>(data), length });
    hb_blob_destroy(blob);
    return buffer;
}

FontPlatformData FontPlatformData::create(const Attributes& data, const FontCustomPlatformData* custom)
{
    Vector<hb_feature_t> features = data.m_features;
    if (custom)
        return { custom->m_typeface.ptr(), data.m_metadata, WTF::move(features), custom };
    auto typeface = TrinityTypeface::matchSystem(data.m_familyName, data.m_weight, data.m_width, data.m_slope);
    return { WTF::move(typeface), data.m_metadata, WTF::move(features) };
}

FontPlatformData::Attributes FontPlatformData::attributes() const
{
    Vector<hb_feature_t> features = m_features;
    if (!m_typeface)
        return { m_metadata, String(), 400, 5, 0, WTF::move(features) };
    return { m_metadata, m_typeface->familyName(), m_typeface->weight(), m_typeface->width(), m_typeface->slope(), WTF::move(features) };
}

#if ENABLE(MATHML)
HbUniquePtr<hb_font_t> FontPlatformData::createOpenTypeMathHarfBuzzFont() const
{
    auto* face = m_typeface ? m_typeface->hbFace() : nullptr;
    if (!face || !hb_ot_math_has_data(face))
        return nullptr;
    return HbUniquePtr<hb_font_t>(hb_font_create(face));
}
#endif

void FontPlatformData::updateSize(float size)
{
    m_metadata.pointSize = size;
    platformDataInit();
}

Vector<FontPlatformData::FontVariationAxis> FontPlatformData::variationAxes(ShouldLocalizeAxisNames) const
{
    if (!m_typeface)
        return { };
    return WTF::map(defaultFontVariationValues(*m_typeface), [](auto&& entry) {
        auto& [tag, values] = entry;
        return FontPlatformData::FontVariationAxis { values.axisName, String(tag), values.defaultValue, values.minimumValue, values.maximumValue };
    });
}

} // namespace WebCore

#endif // USE(TRINITY)
