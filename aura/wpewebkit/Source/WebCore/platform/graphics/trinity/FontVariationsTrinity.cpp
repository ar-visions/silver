// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "FontVariationsTrinity.h"

#if USE(TRINITY)

#include <hb-ot.h>

namespace WebCore {

// the face's fvar axes, read through HarfBuzz
FontVariationDefaultsMap defaultFontVariationValues(const TrinityTypeface& typeface)
{
    FontVariationDefaultsMap map;
    auto* face = typeface.hbFace();
    unsigned count = face ? hb_ot_var_get_axis_count(face) : 0;
    if (!count)
        return map;
    Vector<hb_ot_var_axis_info_t> axes(count);
    hb_ot_var_get_axis_infos(face, 0, &count, axes.mutableSpan().data());
    for (unsigned i = 0; i < count; ++i) {
        auto& axis = axes[i];
        FontTag tag = { { static_cast<char>(axis.tag >> 24), static_cast<char>(axis.tag >> 16), static_cast<char>(axis.tag >> 8), static_cast<char>(axis.tag) } };
        char name[128];
        unsigned length = sizeof(name);
        hb_ot_name_get_utf8(face, axis.name_id, HB_LANGUAGE_INVALID, &length, name);
        map.set(tag, FontVariationDefaults { String::fromUTF8(std::span(name, length)), axis.default_value, axis.min_value, axis.max_value });
    }
    return map;
}

} // namespace WebCore

#endif // USE(TRINITY)
