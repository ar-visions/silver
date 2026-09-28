// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#if USE(TRINITY)

#include "FontDescription.h"
#include "TrinityTypeface.h"

namespace WebCore {

struct FontVariationDefaults {
    float clamp(float value) const
    {
        ASSERT(minimumValue <= maximumValue);
        return std::clamp(value, minimumValue, maximumValue);
    }

    String axisName;
    float defaultValue;
    float minimumValue;
    float maximumValue;
};

using FontVariationDefaultsMap = HashMap<FontTag, FontVariationDefaults, FourCharacterTagHash, FourCharacterTagHashTraits>;
FontVariationDefaultsMap defaultFontVariationValues(const TrinityTypeface&);

} // namespace WebCore

#endif // USE(TRINITY)
