// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "FontRenderOptions.h"

#if USE(TRINITY)

namespace WebCore {

FontRenderOptions& FontRenderOptions::singleton()
{
    static NeverDestroyed<FontRenderOptions> fontRenderOptions;
    return fontRenderOptions;
}

FontRenderOptions::FontRenderOptions() = default;

void FontRenderOptions::setHinting(std::optional<Hinting> hinting)
{
    if (m_isHintingDisabledForTesting)
        return;
    m_hinting = hinting.value_or(Hinting::Slight);
}

void FontRenderOptions::setAntialias(std::optional<Antialias> antialias)
{
    m_antialias = antialias.value_or(Antialias::Normal);
}

void FontRenderOptions::setSubpixelOrder(std::optional<SubpixelOrder> subpixelOrder)
{
    m_subpixelOrder = subpixelOrder.value_or(SubpixelOrder::Unknown);
}

void FontRenderOptions::disableHintingForTesting()
{
    m_hinting = Hinting::None;
    m_isHintingDisabledForTesting = true;
}

} // namespace WebCore

#endif // USE(TRINITY)
