/*
 * Copyright (c) 2024, Aliaksandr Kalenik <kalenik.aliaksandr@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteBuffer.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/PaintingSurface.h>
#include <LibGfx/SharedImageBuffer.h>
#include <LibGfx/WebGfx.h>

namespace Gfx {

struct PaintingSurface::Impl {
    AK_ALLOC_WITH_KMALLOC;

    int canvas { 0 };
    IntSize size;
    RefPtr<Bitmap> bitmap;
};

static bool is_bgr(BitmapFormat format)
{
    return format == BitmapFormat::BGRA8888 || format == BitmapFormat::BGRx8888;
}

static bool has_alpha(BitmapFormat format)
{
    return format == BitmapFormat::BGRA8888 || format == BitmapFormat::RGBA8888;
}

ByteBuffer rgba_from_bitmap(Bitmap const& bitmap)
{
    auto width = bitmap.width();
    auto height = bitmap.height();
    auto rgba = MUST(ByteBuffer::create_uninitialized(static_cast<size_t>(width) * height * 4));
    auto bgr = is_bgr(bitmap.format());
    auto alpha = has_alpha(bitmap.format());
    auto premultiplied = bitmap.alpha_type() == AlphaType::Premultiplied;
    for (int y = 0; y < height; ++y) {
        auto const* src = bitmap.scanline_u8(y);
        auto* dst = rgba.data() + static_cast<size_t>(y) * width * 4;
        for (int x = 0; x < width; ++x, src += 4, dst += 4) {
            u8 a = alpha ? src[3] : 255;
            u8 r = bgr ? src[2] : src[0];
            u8 g = src[1];
            u8 b = bgr ? src[0] : src[2];
            if (premultiplied && a != 0 && a != 255) {
                r = static_cast<u8>(min(255, r * 255 / a));
                g = static_cast<u8>(min(255, g * 255 / a));
                b = static_cast<u8>(min(255, b * 255 / a));
            }
            dst[0] = r;
            dst[1] = g;
            dst[2] = b;
            dst[3] = a;
        }
    }
    return rgba;
}

void rgba_into_bitmap(ReadonlyBytes rgba, IntSize rgba_size, Bitmap& bitmap, IntPoint source_position)
{
    auto bgr = is_bgr(bitmap.format());
    auto alpha = has_alpha(bitmap.format());
    auto premultiplied = bitmap.alpha_type() == AlphaType::Premultiplied;
    for (int y = 0; y < bitmap.height(); ++y) {
        auto sy = y + source_position.y();
        if (sy < 0 || sy >= rgba_size.height())
            continue;
        auto* dst = bitmap.scanline_u8(y);
        for (int x = 0; x < bitmap.width(); ++x, dst += 4) {
            auto sx = x + source_position.x();
            if (sx < 0 || sx >= rgba_size.width())
                continue;
            auto const* src = rgba.data() + (static_cast<size_t>(sy) * rgba_size.width() + sx) * 4;
            u8 a = alpha ? src[3] : 255;
            u8 r = src[0];
            u8 g = src[1];
            u8 b = src[2];
            if (premultiplied && a != 255) {
                r = static_cast<u8>(r * a / 255);
                g = static_cast<u8>(g * a / 255);
                b = static_cast<u8>(b * a / 255);
            }
            dst[0] = bgr ? b : r;
            dst[1] = g;
            dst[2] = bgr ? r : b;
            dst[3] = a;
        }
    }
}

NonnullRefPtr<PaintingSurface> PaintingSurface::create_with_size(IntSize size, BitmapFormat, AlphaType)
{
    auto canvas = webgfx_canvas_new(size.width(), size.height());
    return adopt_ref(*new PaintingSurface(make<Impl>(canvas, size, nullptr)));
}

NonnullRefPtr<PaintingSurface> PaintingSurface::wrap_bitmap(Bitmap& bitmap)
{
    auto canvas = webgfx_canvas_new(bitmap.width(), bitmap.height());
    auto surface = adopt_ref(*new PaintingSurface(make<Impl>(canvas, bitmap.size(), bitmap)));
    surface->write_from_bitmap(bitmap);
    return surface;
}

PaintingSurface::PaintingSurface(NonnullOwnPtr<Impl>&& impl)
    : m_impl(move(impl))
{
}

PaintingSurface::~PaintingSurface()
{
    webgfx_canvas_free(m_impl->canvas);
}

NonnullRefPtr<Bitmap> PaintingSurface::snapshot_bitmap() const
{
    auto bitmap = MUST(Bitmap::create(BitmapFormat::BGRA8888, AlphaType::Premultiplied, size()));
    read_into_bitmap(*bitmap);
    return bitmap;
}

SharedImage PaintingSurface::snapshot_into_shared_image() const
{
    auto shared_image_buffer = SharedImageBuffer::create(size());
    read_into_bitmap(*shared_image_buffer.bitmap());
    return shared_image_buffer.export_shared_image();
}

void PaintingSurface::read_into_bitmap(Bitmap& bitmap, IntPoint source_position) const
{
    auto rgba = MUST(ByteBuffer::create_uninitialized(static_cast<size_t>(size().width()) * size().height() * 4));
    webgfx_canvas_read(m_impl->canvas, rgba.data());
    rgba_into_bitmap(rgba, size(), bitmap, source_position);
}

void PaintingSurface::write_from_bitmap(Bitmap const& bitmap)
{
    float const clear[4] = { 0, 0, 0, 0 };
    webgfx_canvas_clear(m_impl->canvas, clear);
    auto rgba = rgba_from_bitmap(bitmap);
    float const dst[4] = { 0, 0, static_cast<float>(bitmap.width()), static_cast<float>(bitmap.height()) };
    float const uv[4] = { 0, 0, 1, 1 };
    webgfx_canvas_image(m_impl->canvas, rgba.data(), bitmap.width(), bitmap.height(), dst, uv);
    // a wrapped bitmap is this surface's memory to its owner
    if (m_impl->bitmap && m_impl->bitmap.ptr() != &bitmap)
        rgba_into_bitmap(rgba, bitmap.size(), *m_impl->bitmap);
}

void PaintingSurface::copy_from_surface(PaintingSurface& source)
{
    source.flush();
    auto bitmap = source.snapshot_bitmap();
    write_from_bitmap(*bitmap);
}

IntSize PaintingSurface::size() const
{
    return m_impl->size;
}

IntRect PaintingSurface::rect() const
{
    return { {}, m_impl->size };
}

int PaintingSurface::webgfx_canvas() const
{
    return m_impl->canvas;
}

void PaintingSurface::notify_content_will_change()
{
}

void PaintingSurface::flush()
{
    // a wrapped bitmap gets the drawing back
    if (m_impl->bitmap)
        read_into_bitmap(*m_impl->bitmap);
    if (on_flush)
        on_flush(*this);
}

}
