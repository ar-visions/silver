// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "Gradient.h"

#if USE(TRINITY)

#include "GraphicsContextTrinity.h"

namespace WebCore {

void Gradient::stopsChanged()
{
}

void Gradient::fill(GraphicsContext& context, const FloatRect& rect)
{
    context.fillRect(rect, *this, context.fillGradientSpaceTransform());
}

} // namespace WebCore

#endif // USE(TRINITY)
