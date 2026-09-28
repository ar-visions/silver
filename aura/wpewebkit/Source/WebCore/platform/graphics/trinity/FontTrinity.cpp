// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "Font.h"

#if USE(TRINITY)

#include "FontCascade.h"
#include "Path.h"
#include <hb-ot.h>

namespace WebCore {

static inline float fromHarfBuzz(hb_position_t value)
{
    return static_cast<float>(value) / (1 << 16);
}

// HarfBuzz outlines into a WebKit path, y down
static hb_draw_funcs_t* pathDrawFuncs()
{
    static hb_draw_funcs_t* funcs = [] {
        auto* funcs = hb_draw_funcs_create();
        hb_draw_funcs_set_move_to_func(funcs, [](hb_draw_funcs_t*, void* path, hb_draw_state_t*, float x, float y, void*) {
            static_cast<Path*>(path)->moveTo({ x, -y });
        }, nullptr, nullptr);
        hb_draw_funcs_set_line_to_func(funcs, [](hb_draw_funcs_t*, void* path, hb_draw_state_t*, float x, float y, void*) {
            static_cast<Path*>(path)->addLineTo({ x, -y });
        }, nullptr, nullptr);
        hb_draw_funcs_set_quadratic_to_func(funcs, [](hb_draw_funcs_t*, void* path, hb_draw_state_t*, float cx, float cy, float x, float y, void*) {
            static_cast<Path*>(path)->addQuadCurveTo({ cx, -cy }, { x, -y });
        }, nullptr, nullptr);
        hb_draw_funcs_set_cubic_to_func(funcs, [](hb_draw_funcs_t*, void* path, hb_draw_state_t*, float c1x, float c1y, float c2x, float c2y, float x, float y, void*) {
            static_cast<Path*>(path)->addBezierCurveTo({ c1x, -c1y }, { c2x, -c2y }, { x, -y });
        }, nullptr, nullptr);
        hb_draw_funcs_set_close_path_func(funcs, [](hb_draw_funcs_t*, void* path, hb_draw_state_t*, void*) {
            static_cast<Path*>(path)->closeSubpath();
        }, nullptr, nullptr);
        hb_draw_funcs_make_immutable(funcs);
        return funcs;
    }();
    return funcs;
}

Path Font::platformPathForGlyph(Glyph glyph) const
{
    auto* font = m_platformData.hbFont();
    if (!font)
        return { };
    Path path;
    hb_font_draw_glyph(font, glyph, pathDrawFuncs(), &path);
    // HarfBuzz draws in 16.16 units: back to pixels
    path.transform(AffineTransform::makeScale({ 1.0 / (1 << 16), 1.0 / (1 << 16) }));
    return path;
}

FloatRect Font::platformBoundsForGlyph(Glyph glyph) const
{
    auto* font = m_platformData.hbFont();
    hb_glyph_extents_t extents;
    if (!font || !m_platformData.size() || !hb_font_get_glyph_extents(font, glyph, &extents))
        return { };
    return { fromHarfBuzz(extents.x_bearing), -fromHarfBuzz(extents.y_bearing), fromHarfBuzz(extents.width), -fromHarfBuzz(extents.height) };
}

Vector<FloatRect, Font::inlineGlyphRunCapacity> Font::platformBoundsForGlyphs(const Vector<Glyph, inlineGlyphRunCapacity>& glyphs) const
{
    return glyphs.map<Vector<FloatRect, inlineGlyphRunCapacity>>([&](Glyph glyph) {
        return platformBoundsForGlyph(glyph);
    });
}

float Font::platformWidthForGlyph(Glyph glyph) const
{
    auto* font = m_platformData.hbFont();
    if (!font || !m_platformData.size())
        return 0;
    return fromHarfBuzz(hb_font_get_glyph_h_advance(font, glyph));
}

void Font::platformInit()
{
    auto* font = m_platformData.hbFont();
    if (!font || !m_platformData.size())
        return;

    hb_font_extents_t extents;
    hb_font_get_h_extents(font, &extents);
    auto ascent = std::round(fromHarfBuzz(extents.ascender));
    auto descent = std::round(-fromHarfBuzz(extents.descender));
    auto lineGap = fromHarfBuzz(extents.line_gap);
    m_fontMetrics.setAscent(ascent);
    m_fontMetrics.setDescent(descent);
    m_fontMetrics.setLineGap(lineGap);
    m_fontMetrics.setLineSpacing(lroundf(ascent) + lroundf(descent) + lroundf(lineGap));

    hb_position_t position;
    if (hb_ot_metrics_get_position(font, HB_OT_METRICS_TAG_CAP_HEIGHT, &position))
        m_fontMetrics.setCapHeight(fromHarfBuzz(position));
    if (hb_ot_metrics_get_position(font, HB_OT_METRICS_TAG_X_HEIGHT, &position) && position)
        m_fontMetrics.setXHeight(fromHarfBuzz(position));
    if (hb_ot_metrics_get_position(font, HB_OT_METRICS_TAG_UNDERLINE_OFFSET, &position))
        m_fontMetrics.setUnderlinePosition(-fromHarfBuzz(position));
    if (hb_ot_metrics_get_position(font, HB_OT_METRICS_TAG_UNDERLINE_SIZE, &position))
        m_fontMetrics.setUnderlineThickness(fromHarfBuzz(position));

    m_fontMetrics.setUnitsPerEm(m_platformData.typeface()->unitsPerEm());

    if (m_platformData.isColorBitmapFont())
        m_emojiType = AllEmojiGlyphs { };
    else
        m_emojiType = NoEmojiGlyphs { };

    if (equalIgnoringASCIICase(m_platformData.familyName(), "Ahem"_s))
        m_allowsAntialiasing = false;
}

void Font::platformCharWidthInit()
{
    m_avgCharWidth = 0.f;
    m_maxCharWidth = 0.f;
    initCharWidths();
}

RefPtr<Font> Font::platformCreateScaledFont(const FontDescription&, float scaleFactor) const
{
    return Font::create(FontPlatformData(m_platformData.typeface(), scaleFactor * m_platformData.size(),
        m_platformData.syntheticBold(),
        m_platformData.syntheticOblique(),
        m_platformData.orientation(),
        m_platformData.widthVariant(),
        m_platformData.textRenderingMode(),
        Vector<hb_feature_t> { m_platformData.features() },
        m_platformData.customPlatformData()),
        origin(), IsInterstitial::No);
}

RefPtr<Font> Font::platformCreateHalfWidthFont() const
{
    return Font::create(FontPlatformData(m_platformData.typeface(), m_platformData.size(),
        m_platformData.syntheticBold(),
        m_platformData.syntheticOblique(),
        m_platformData.orientation(),
        FontWidthVariant::HalfWidth,
        m_platformData.textRenderingMode(),
        Vector<hb_feature_t> { m_platformData.features() },
        m_platformData.customPlatformData()),
        origin(), IsInterstitial::No);
}

void Font::determinePitch()
{
    m_treatAsFixedPitch = m_platformData.isFixedPitch();
}

bool Font::variantCapsSupportedForSynthesis(FontVariantCaps fontVariantCaps) const
{
    switch (fontVariantCaps) {
    case FontVariantCaps::Small:
    case FontVariantCaps::Petite:
    case FontVariantCaps::AllSmall:
    case FontVariantCaps::AllPetite:
        return false;
    default:
        return true;
    }
}

bool Font::platformSupportsCodePoint(char32_t character, std::optional<char32_t> variation) const
{
    auto* font = m_platformData.hbFont();
    if (!font)
        return false;
    // the font's glyph funcs apply WebKit's space rules
    hb_codepoint_t glyph;
    return hb_font_get_glyph(font, character, variation.value_or(0), &glyph);
}

} // namespace WebCore

#endif // USE(TRINITY)
