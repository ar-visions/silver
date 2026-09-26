/*
 * Copyright (c) 2024, Andreas Kling <andreas@ladybird.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGfx/Bitmap.h>
#include <LibGfx/Painter.h>
#include <LibGfx/PainterTrinity.h>
#include <LibGfx/PaintingSurface.h>

namespace Gfx {

Painter::~Painter() = default;

NonnullOwnPtr<Painter> Painter::create(NonnullRefPtr<Gfx::Bitmap> target_bitmap)
{
    auto painting_surface = PaintingSurface::wrap_bitmap(target_bitmap);
    return make<PainterTrinity>(painting_surface);
}

}
