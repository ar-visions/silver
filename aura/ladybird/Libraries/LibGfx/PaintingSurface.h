/*
 * Copyright (c) 2024, Aliaksandr Kalenik <kalenik.aliaksandr@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/AtomicRefCounted.h>
#include <AK/Function.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/RefPtr.h>
#include <LibGfx/Color.h>
#include <LibGfx/Forward.h>
#include <LibGfx/Size.h>

namespace Gfx {

class SharedImage;

// a trinity Canvas, held by its webgfx id
class PaintingSurface : public AtomicRefCounted<PaintingSurface> {
public:
    enum class Origin {
        TopLeft,
        BottomLeft,
    };

    Function<void(PaintingSurface&)> on_flush;

    static NonnullRefPtr<PaintingSurface> create_with_size(IntSize size, BitmapFormat color_type, AlphaType alpha_type);
    static NonnullRefPtr<PaintingSurface> wrap_bitmap(Bitmap&);

    NonnullRefPtr<Bitmap> snapshot_bitmap() const;
    SharedImage snapshot_into_shared_image() const;

    void read_into_bitmap(Bitmap&, IntPoint source_position = {}) const;
    void write_from_bitmap(Bitmap const&);
    void copy_from_surface(PaintingSurface&);

    void notify_content_will_change();

    IntSize size() const;
    IntRect rect() const;

    int webgfx_canvas() const;

    void flush();

    ~PaintingSurface();

private:
    struct Impl;

    PaintingSurface(NonnullOwnPtr<Impl>&&);

    NonnullOwnPtr<Impl> m_impl;
};

// rgba8 straight alpha, as webgfx reads and writes it
ByteBuffer rgba_from_bitmap(Bitmap const&);
void rgba_into_bitmap(ReadonlyBytes rgba, IntSize rgba_size, Bitmap&, IntPoint source_position = {});

}
