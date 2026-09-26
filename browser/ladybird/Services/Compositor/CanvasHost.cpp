/*
 * Copyright (c) 2026, Aliaksandr Kalenik <kalenik.aliaksandr@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Compositor/CanvasHost.h>
#include <LibCompositing/DisplayList/Canvas2DCommandStream.h>
#include <LibCompositing/DisplayList/CanvasSurfaceRegistry.h>
#include <LibCompositing/DisplayList/DisplayList.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/CanvasCommandPlayer.h>
#include <LibGfx/PaintingSurface.h>
#include <LibGfx/Font/Font.h>

namespace Compositor {

// WebGL is not ported: no GL context, so every WebGL call fails
static void webgl_not_ported()
{
    dbgln("trinity compositor: not ported: WebGL");
}

CanvasHost::CanvasHost(Compositing::CanvasSurfaceRegistry& canvas_surface_registry)
    : m_canvas_surface_registry(canvas_surface_registry)
{
}

CanvasHost::~CanvasHost()
{
    for (auto canvas_id : m_contexts.keys())
        m_canvas_surface_registry.remove_canvas_surface(canvas_id);
}

OwnPtr<Gfx::CanvasCommandPlayer> CanvasHost::create_2d_command_player(Gfx::IntSize size, bool alpha)
{
    if (size.is_empty() || static_cast<i64>(size.width()) * static_cast<i64>(size.height()) > Gfx::max_canvas_area)
        return nullptr;

    auto format = alpha ? Gfx::BitmapFormat::BGRA8888 : Gfx::BitmapFormat::BGRx8888;
    auto canvas_surface_resolver = [this](u64 canvas_id) -> Gfx::PaintingSurface const* {
        // A 2D source resolves to its live draw surface: the shared command
        // stream replays in recording order, so at this point the surface holds
        // exactly the commands recorded before the referencing DrawCanvas.
        if (auto* context = this->context(Compositing::CanvasId { canvas_id })) {
            if (auto* canvas_context = context->get_pointer<Canvas2DContext>())
                return canvas_context->command_player->surface().ptr();
        }
        // WebGL sources are presented separately and resolve via the registry.
        return m_canvas_surface_registry.canvas_surface(Compositing::CanvasId { canvas_id });
    };
    auto font_resolver = [this](u64 font_id) -> Gfx::Font const* {
        if (!m_text_resources.has_font(Compositing::FontResourceId { font_id }))
            return nullptr;
        return &m_text_resources.font(Compositing::FontResourceId { font_id });
    };
    auto player = make<Gfx::CanvasCommandPlayer>(size, format, Gfx::AlphaType::Premultiplied, move(canvas_surface_resolver), move(font_resolver));

    // https://html.spec.whatwg.org/multipage/canvas.html#the-canvas-settings:concept-canvas-alpha
    // "Thus, the bitmap of such a context starts off as opaque black instead of transparent black"
    // AD-HOC: a new surface starts fully transparent; only clear when alpha is disabled.
    if (!alpha)
        player->clear(Gfx::Color::Black);

    return player;
}

static void copy_surface_contents(Gfx::PaintingSurface& source, Gfx::PaintingSurface& destination)
{
    destination.copy_from_surface(source);
}

static NonnullRefPtr<Gfx::PaintingSurface> create_presented_canvas_surface(Gfx::PaintingSurface& source)
{
    auto surface = Gfx::PaintingSurface::create_with_size(
        source.size(),
        Gfx::BitmapFormat::BGRA8888,
        Gfx::AlphaType::Premultiplied);
    copy_surface_contents(source, *surface);
    return surface;
}

Optional<Compositing::CanvasId> CanvasHost::create_2d_context(Gfx::IntSize size, bool alpha)
{
    auto command_player = create_2d_command_player(size, alpha);
    if (!command_player)
        return {};

    auto presented_surface = create_presented_canvas_surface(command_player->surface());
    auto canvas_id = m_canvas_surface_registry.create_canvas_surface(presented_surface);
    Canvas2DContext context {
        .command_player = command_player.release_nonnull(),
        .presented_surface = move(presented_surface),
    };
    m_contexts.set(canvas_id, move(context));
    return canvas_id;
}

CanvasHost::CreateWebGLContextResult CanvasHost::create_webgl_context(Compositing::WebGL::WebGLVersion, Gfx::IntSize, bool, bool, bool)
{
    webgl_not_ported();
    return {};
}

void CanvasHost::destroy_context(Compositing::CanvasId canvas_id)
{
    m_contexts.remove(canvas_id);
    m_canvas_surface_registry.remove_canvas_surface(canvas_id);
}

bool CanvasHost::has_context(Compositing::CanvasId canvas_id) const
{
    return m_contexts.contains(canvas_id);
}

CanvasHost::Context* CanvasHost::context(Compositing::CanvasId canvas_id)
{
    auto it = m_contexts.find(canvas_id);
    if (it == m_contexts.end())
        return nullptr;
    return &it->value;
}

void CanvasHost::present_canvas_2d_context(Compositing::CanvasId canvas_id, Canvas2DContext& context)
{
    copy_surface_contents(context.command_player->surface(), context.presented_surface);
    m_canvas_surface_registry.set_canvas_surface(canvas_id, context.presented_surface);
    context.has_uncommitted_commands = false;
}

void CanvasHost::execute_canvas_2d_stream(Vector<Compositing::Canvas2DCommandStreamSegment> const& segments, Vector<Compositing::DisplayListFontResource> const& fonts)
{
    Compositing::DisplayListResourceSet resources;
    for (auto const& font : fonts) {
        // NB: Font IDs are immutable. Preserve the backing storage used by cached text blobs.
        if (!m_text_resources.has_font(font.id))
            m_text_resources.set_font(font.id, font.font);
        resources.fonts.set(font.id);
    }
    m_text_resources.retain_only(resources);
    for (auto const& segment : segments) {
        // The canvas may have been destroyed while this segment was pending in
        // WebContent, so a missing context is not a protocol violation.
        auto* context = this->context(segment.canvas_id);
        auto* canvas_context = context ? context->get_pointer<Canvas2DContext>() : nullptr;
        if (!canvas_context)
            continue;

        if (!segment.commands.is_empty()) {
            canvas_context->command_player->play(segment.commands);
            canvas_context->has_uncommitted_commands = true;
        }

        if (segment.present && canvas_context->has_uncommitted_commands)
            present_canvas_2d_context(segment.canvas_id, *canvas_context);
    }
}

void CanvasHost::execute_webgl_commands(Compositing::CanvasId, ReadonlyBytes, Vector<Gfx::DecodedImageFrame> const&)
{
    webgl_not_ported();
}

void CanvasHost::set_webgl_shared_command_buffer(Compositing::CanvasId, Compositing::WebGL::WebGLSharedCommandBuffer)
{
    webgl_not_ported();
}

bool CanvasHost::execute_webgl_commands_from_shared_buffer(Compositing::CanvasId, u64, u64, u64, Vector<Gfx::DecodedImageFrame> const&)
{
    webgl_not_ported();
    return false;
}

ErrorOr<ByteBuffer> CanvasHost::execute_webgl_sync_call(Compositing::CanvasId, ByteBuffer)
{
    webgl_not_ported();
    return Error::from_string_literal("WebGL is not ported");
}

Compositing::WebGL::ReadPixelsResult CanvasHost::webgl_read_pixels_robust_angle(Compositing::CanvasId, Compositing::WebGL::GLint, Compositing::WebGL::GLint, Compositing::WebGL::GLsizei, Compositing::WebGL::GLsizei, Compositing::WebGL::GLenum, Compositing::WebGL::GLenum, Compositing::WebGL::GLsizei, Core::AnonymousBuffer)
{
    webgl_not_ported();
    return {};
}

bool CanvasHost::webgl_read_buffer_sub_data(Compositing::CanvasId, Compositing::WebGL::GLenum, Compositing::WebGL::GLintptr, Compositing::WebGL::GLintptr, Core::AnonymousBuffer)
{
    webgl_not_ported();
    return false;
}

void CanvasHost::present_webgl_canvas(Compositing::CanvasId, bool)
{
    webgl_not_ported();
}

static Gfx::ShareableBitmap read_back_surface(Gfx::PaintingSurface& surface, Gfx::IntRect rect)
{
    auto clipped_rect = rect.intersected(surface.rect());
    if (clipped_rect.is_empty())
        return {};

    auto bitmap_or_error = Gfx::Bitmap::create_shareable(Gfx::BitmapFormat::BGRA8888, Gfx::AlphaType::Premultiplied, clipped_rect.size());
    if (bitmap_or_error.is_error())
        return {};

    auto bitmap = bitmap_or_error.release_value();
    surface.flush();
    surface.read_into_bitmap(*bitmap, clipped_rect.location());
    return Gfx::ShareableBitmap { move(bitmap), Gfx::ShareableBitmap::ConstructWithKnownGoodBitmap };
}

Gfx::ShareableBitmap CanvasHost::read_back_pixels(Compositing::CanvasId canvas_id, Gfx::IntRect rect)
{
    auto* context = this->context(canvas_id);
    if (!context)
        return {};

    return read_back_surface(context->get<Canvas2DContext>().command_player->surface(), rect);
}

}
