// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include "FontCascade.h"

namespace WebCore {

inline constexpr bool FontCascade::canReturnFallbackFontsForComplexText()
{
    return false;
}

inline constexpr bool FontCascade::canExpandAroundIdeographsInComplexText()
{
    return false;
}

}
