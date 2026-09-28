/*
 * Copyright (c) 2024-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGfx/Bitmap.h>
#include <LibGfx/BitmapExport.h>
#include <LibGfx/ColorSpace.h>

namespace Gfx {

StringView export_format_name(ExportFormat format)
{
    switch (format) {
#define ENUMERATE_EXPORT_FORMAT(format) \
    case Gfx::ExportFormat::format:     \
        return #format##sv;
        ENUMERATE_EXPORT_FORMATS(ENUMERATE_EXPORT_FORMAT)
#undef ENUMERATE_EXPORT_FORMAT
    }
    VERIFY_NOT_REACHED();
}

static int bytes_per_pixel_for_export_format(ExportFormat format)
{
    switch (format) {
    case ExportFormat::Gray8:
    case ExportFormat::Alpha8:
        return 1;
    case ExportFormat::RGB565:
    case ExportFormat::RGBA5551:
    case ExportFormat::RGBA4444:
        return 2;
    case ExportFormat::RGB888:
        return 3;
    case ExportFormat::RGBA8888:
        return 4;
    default:
        VERIFY_NOT_REACHED();
    }
}

// one pixel in the export format; alpha straight unless asked
static void pack_pixel(u8* out, ExportFormat format, Color color, bool premultiply)
{
    u8 r = color.red(), g = color.green(), b = color.blue(), a = color.alpha();
    if (premultiply && a != 255) {
        r = static_cast<u8>(r * a / 255);
        g = static_cast<u8>(g * a / 255);
        b = static_cast<u8>(b * a / 255);
    }
    switch (format) {
    case ExportFormat::Gray8:
        out[0] = static_cast<u8>((r * 54 + g * 183 + b * 19) >> 8);
        break;
    case ExportFormat::Alpha8:
        out[0] = a;
        break;
    case ExportFormat::RGB565: {
        u16 v = static_cast<u16>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        out[0] = v & 0xff;
        out[1] = v >> 8;
        break;
    }
    case ExportFormat::RGBA5551: {
        u16 v = static_cast<u16>(((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (a >> 7));
        out[0] = v & 0xff;
        out[1] = v >> 8;
        break;
    }
    case ExportFormat::RGBA4444: {
        u16 v = static_cast<u16>(((r >> 4) << 12) | ((g >> 4) << 8) | ((b >> 4) << 4) | (a >> 4));
        out[0] = v & 0xff;
        out[1] = v >> 8;
        break;
    }
    case ExportFormat::RGB888:
        out[0] = r;
        out[1] = g;
        out[2] = b;
        break;
    case ExportFormat::RGBA8888:
        out[0] = r;
        out[1] = g;
        out[2] = b;
        out[3] = a;
        break;
    default:
        VERIFY_NOT_REACHED();
    }
}

ErrorOr<BitmapExportResult> export_bitmap_to_byte_buffer(
    Bitmap const& bitmap,
    ColorSpace const& color_space,
    ExportFormat format,
    int flags,
    Optional<int> target_width,
    Optional<int> target_height)
{
    int width = target_width.value_or(bitmap.width());
    int height = target_height.value_or(bitmap.height());

    Checked<size_t> buffer_pitch = width;
    int number_of_bytes = bytes_per_pixel_for_export_format(format);
    buffer_pitch *= number_of_bytes;
    if (buffer_pitch.has_overflow())
        return Error::from_string_literal("Gfx::export_bitmap_to_byte_buffer size overflow");

    if (Checked<size_t>::multiplication_would_overflow(buffer_pitch.value(), height))
        return Error::from_string_literal("Gfx::export_bitmap_to_byte_buffer size overflow");

    auto buffer = MUST(ByteBuffer::create_zeroed(buffer_pitch.value() * height));

    if (width > 0 && height > 0) {
        (void)color_space;
        auto* raw_buffer = buffer.data();
        for (auto y = 0; y < height; y++) {
            auto target_y = flags & ExportFlags::FlipY ? height - y - 1 : y;
            auto source_y = y * bitmap.height() / height;
            for (auto x = 0; x < width; x++) {
                auto pixel = bitmap.get_pixel(x * bitmap.width() / width, source_y);
                auto* out = raw_buffer + target_y * buffer_pitch.value() + static_cast<size_t>(x) * number_of_bytes;
                pack_pixel(out, format, pixel, flags & ExportFlags::PremultiplyAlpha);
            }
        }
    } else {
        VERIFY(buffer.is_empty());
    }

    return BitmapExportResult {
        .buffer = move(buffer),
        .width = width,
        .height = height,
    };
}

}
