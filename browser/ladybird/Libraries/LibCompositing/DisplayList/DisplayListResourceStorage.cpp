/*
 * Copyright (c) 2026, Aliaksandr Kalenik <kalenik.aliaksandr@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/BitCast.h>
#include <AK/ByteBuffer.h>
#include <AK/Function.h>
#include <AK/Math.h>
#include <LibCompositing/DisplayList/DisplayList.h>
#include <LibCompositing/DisplayList/DisplayListResourceStorage.h>
#include <LibCompositing/RustFFI.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/ColorSpace.h>
#include <LibGfx/Filter.h>
#include <LibGfx/Font/Font.h>
#include <LibGfx/PaintingSurface.h>
#include <LibGfx/WebGfx.h>
#include <LibGfx/YUVData.h>
#include <LibMedia/VideoFrame.h>
#include <LibMedia/VideoFrameHandle.h>
#include <LibMedia/VideoFrameHandle.h>
#include <LibMedia/VideoSurface.h>

namespace Compositing {

struct DisplayListStoredImageFrameResource {
    AK_ALLOC_WITH_KMALLOC;

    explicit DisplayListStoredImageFrameResource(Gfx::DecodedImageFrame frame)
        : frame(move(frame))
    {
    }

    ~DisplayListStoredImageFrameResource()
    {
        if (webgfx_image != 0)
            webgfx_image_free(webgfx_image);
    }

    Gfx::DecodedImageFrame frame;
    // Classifying costs a walk over the pixels and the answer never changes for a given frame — so it's worked out on
    // first use, and kept.
    mutable Optional<bool> force_dark_should_filter;
    // the frame's GPU texture, uploaded on its first draw
    mutable int webgfx_image { 0 };
};

struct DisplayListStoredVideoSinkResource {
    AK_ALLOC_WITH_KMALLOC;

    ~DisplayListStoredVideoSinkResource()
    {
        free_textures();
    }

    void free_textures()
    {
        for (auto id : { video.rgba, video.y, video.u, video.v }) {
            if (id != 0)
                webgfx_image_free(id);
        }
        video = {};
    }

    RefPtr<Media::VideoSink> sink;
    // the textures each new frame is uploaded into
    VideoTextures video;
    Gfx::IntSize chroma_size;
    bool planar { false };
    Media::VideoFramePoolID pool_id { 0 };
    u32 slot_index { 0 };
    u64 slot_acquisition_id { 0 };
};

bool DisplayListResourceSet::is_empty() const
{
    return fonts.is_empty()
        && image_frames.is_empty()
        && video_sinks.is_empty()
        && display_lists.is_empty();
}

void DisplayListResourceSet::include(DisplayListResourceSet const& other)
{
    for (auto id : other.fonts)
        fonts.set(id, AK::HashSetExistingEntryBehavior::Keep);
    for (auto id : other.image_frames)
        image_frames.set(id, AK::HashSetExistingEntryBehavior::Keep);
    for (auto id : other.video_sinks)
        video_sinks.set(id, AK::HashSetExistingEntryBehavior::Keep);
    for (auto id : other.display_lists)
        display_lists.set(id, AK::HashSetExistingEntryBehavior::Keep);
}

DisplayListResource::DisplayListResource(NonnullRefPtr<DisplayList> display_list, AccumulatedVisualContextTree visual_context_tree)
    : display_list(move(display_list))
    , visual_context_tree(move(visual_context_tree))
{
}

DisplayListResource::DisplayListResource(NonnullRefPtr<DisplayList const> display_list, AccumulatedVisualContextTree visual_context_tree)
    : display_list(move(display_list))
    , visual_context_tree(move(visual_context_tree))
{
}

DisplayListResource::DisplayListResource(DisplayList const& display_list, AccumulatedVisualContextTree visual_context_tree)
    : display_list(display_list)
    , visual_context_tree(move(visual_context_tree))
{
}

DisplayListResourceStorage::DisplayListResourceStorage() = default;
DisplayListResourceStorage::DisplayListResourceStorage(DisplayListResourceStorage&&) = default;
DisplayListResourceStorage& DisplayListResourceStorage::operator=(DisplayListResourceStorage&&) = default;
DisplayListResourceStorage::~DisplayListResourceStorage() = default;

FontResourceId DisplayListResourceStorage::add_font(Gfx::Font const& font)
{
    m_has_resources_added_since_last_retain = true;
    auto id = font.id();
    m_fonts.ensure(id, [&]() -> NonnullRefPtr<Gfx::Font const> { return font; });
    return { id };
}

// Coarse by design: the verdict only must tell line art from photos; a large image shouldn't pay per-pixel to be asked.
static constexpr size_t max_sampled_pixels = 1000;

static bool classify_image_frame_for_force_dark(Gfx::DecodedImageFrame const& frame)
{
    auto const& bitmap = frame.bitmap();
    auto size = bitmap.size();
    if (size.is_empty())
        return false;

    auto total = static_cast<double>(size.width()) * static_cast<double>(size.height());
    auto step = max(1, static_cast<int>(AK::ceil(AK::sqrt(total / static_cast<double>(max_sampled_pixels)))));

    Vector<u32> opaque_samples;
    size_t transparent_count = 0;
    size_t sampled_count = 0;
    for (int y = 0; y < size.height(); y += step) {
        for (int x = 0; x < size.width(); x += step) {
            auto color = bitmap.get_pixel(x, y);
            sampled_count++;
            // A pixel this sheer says something about the image's shape rather than its palette — so it's counted
            // toward transparency, but kept out of the palette samples.
            if (color.alpha() < 128) {
                transparent_count++;
                continue;
            }
            opaque_samples.append(color.value());
        }
    }
    if (sampled_count == 0)
        return false;

    auto transparency_ratio = static_cast<float>(transparent_count) / static_cast<float>(sampled_count);
    return Compositing::RustFFI::ladybird_web_force_dark_should_filter_image(
        opaque_samples.data(), opaque_samples.size(), transparency_ratio);
}

bool DisplayListResourceStorage::image_frame_should_force_dark(ImageFrameResourceId id) const
{
    auto stored = m_image_frames.get(id.value());
    if (!stored.has_value())
        return false;
    auto const& resource = *stored.value();
    if (!resource.force_dark_should_filter.has_value())
        resource.force_dark_should_filter = classify_image_frame_for_force_dark(resource.frame);
    return resource.force_dark_should_filter.value();
}

ImageFrameResourceId DisplayListResourceStorage::add_image_frame(Gfx::DecodedImageFrame const& frame)
{
    m_has_resources_added_since_last_retain = true;
    auto id = frame.id();
    m_image_frames.ensure(id, [&] { return make<DisplayListStoredImageFrameResource>(frame); });
    return { id };
}

VideoSinkResourceId DisplayListResourceStorage::add_video_sink(VideoSinkResourceId id, Media::VideoSinkHandle sink_handle)
{
    m_has_resources_added_since_last_retain = true;
    m_video_sink_handles.set(id.value(), sink_handle, AK::HashSetExistingEntryBehavior::Keep);
    return id;
}

DisplayListResourceId DisplayListResourceStorage::add_display_list(NonnullRefPtr<DisplayList const> display_list, AccumulatedVisualContextTree const& visual_context_tree)
{
    m_has_resources_added_since_last_retain = true;
    auto id = display_list->id();
    m_display_lists.ensure(id, [&] {
        return DisplayListResource { move(display_list), visual_context_tree };
    });
    return { id };
}

DisplayListResourceId DisplayListResourceStorage::add_display_list(DisplayListResource&& resource)
{
    m_has_resources_added_since_last_retain = true;
    auto id = resource.display_list->id();
    m_display_lists.set(id, move(resource), AK::HashSetExistingEntryBehavior::Keep);
    return { id };
}

void DisplayListResourceStorage::set_font(FontResourceId id, NonnullRefPtr<Gfx::Font const> font)
{
    m_has_resources_added_since_last_retain = true;
    m_fonts.set(id.value(), move(font));
}

void DisplayListResourceStorage::set_image_frame(ImageFrameResourceId id, Gfx::DecodedImageFrame frame)
{
    m_has_resources_added_since_last_retain = true;
    m_image_frames.set(id.value(), make<DisplayListStoredImageFrameResource>(move(frame)));
}

Gfx::DecodedImageFrame const& DisplayListResourceStorage::image_frame(ImageFrameResourceId id) const
{
    return m_image_frames.get(id.value()).value()->frame;
}

int DisplayListResourceStorage::webgfx_image(ImageFrameResourceId id) const
{
    auto& stored = *m_image_frames.get(id.value()).value();
    if (stored.webgfx_image == 0) {
        auto const& bitmap = stored.frame.bitmap();
        auto rgba = Gfx::rgba_from_bitmap(bitmap);
        stored.webgfx_image = webgfx_image_new(rgba.data(), bitmap.width(), bitmap.height());
    }
    return stored.webgfx_image;
}

static ReadonlyBytes inline_data(ReadonlyBytes payload, DisplayListDataSpan span)
{
    VERIFY(static_cast<size_t>(span.offset) + span.size <= payload.size());
    return payload.slice(span.offset, span.size);
}

template<typename Command, typename Callback>
static void for_each_command_byte_range_inside(Command const& command, ReadonlyBytes payload, Callback&& callback)
{
    if constexpr (IsSame<Command, DrawIsolatedGroup>) {
        callback(inline_data(payload, command.content));
        if (command.mask.size != 0)
            callback(inline_data(payload, command.mask));
    } else if constexpr (IsSame<Command, DrawRepeatedTile>) {
        callback(inline_data(payload, command.tile));
    } else if constexpr (IsSame<Command, DeclareMaskContent>) {
        callback(inline_data(payload, command.content));
    } else if constexpr (requires { command.paint_style; command.paint_kind; }) {
        if (command.paint_kind == decltype(command.paint_kind)::PaintStyle
            && command.paint_style.paint_style_type == DisplayListPaintStyleType::Pattern)
            callback(inline_data(payload, command.paint_style.pattern_tile));
    }
}

void DisplayListResourceStorage::collect_referenced_resources(
    ReadonlyBytes command_bytes,
    DisplayListResourceSet& referenced_resources) const
{
    auto add_display_list_resource = [&](DisplayListResourceId id) {
        add_referenced_display_list(id, referenced_resources);
    };

    DisplayList::for_each_command_header(command_bytes, [&](DisplayListCommandHeader const& header, ReadonlyBytes payload) {
        visit_display_list_command(header.command_type, payload, [&](auto const& command) {
            using Command = RemoveCVReference<decltype(command)>;
            if constexpr (requires { command.font_id; })
                referenced_resources.fonts.set(command.font_id, AK::HashSetExistingEntryBehavior::Keep);
            if constexpr (requires { command.frame_id; })
                referenced_resources.image_frames.set(command.frame_id, AK::HashSetExistingEntryBehavior::Keep);
            if constexpr (requires { command.video_sink_id; })
                referenced_resources.video_sinks.set(command.video_sink_id, AK::HashSetExistingEntryBehavior::Keep);
            if constexpr (IsSame<Command, DrawIsolatedGroup>) {
                if (command.filter.size != 0) {
                    Gfx::for_each_filter_image_frame_id(inline_data(payload, command.filter), [&](u64 image_id) {
                        referenced_resources.image_frames.set(ImageFrameResourceId { image_id }, AK::HashSetExistingEntryBehavior::Keep);
                    });
                }
            }
            if constexpr (requires { command.display_list_id; }) {
                add_display_list_resource(command.display_list_id);
            }
            for_each_command_byte_range_inside(command, payload, [&](ReadonlyBytes nested_records) {
                collect_referenced_resources(nested_records, referenced_resources);
            });
        });
    });
}

void DisplayListResourceStorage::collect_referenced_resources(
    DisplayList const& display_list,
    DisplayListResourceSet& referenced_resources) const
{
    collect_referenced_resources(display_list.command_bytes(), referenced_resources);
}

void DisplayListResourceStorage::add_referenced_display_list(DisplayListResourceId id, DisplayListResourceSet& referenced_resources) const
{
    if (referenced_resources.display_lists.set(id, AK::HashSetExistingEntryBehavior::Keep) != HashSetResult::InsertedNewEntry)
        return;
    if (!has_display_list(id))
        return;
    collect_referenced_resources(display_list(id), referenced_resources);
    collect_referenced_resources(display_list_visual_context_tree(id), referenced_resources);
}

void DisplayListResourceStorage::collect_referenced_resources(
    AccumulatedVisualContextTree const& visual_context_tree,
    DisplayListResourceSet& referenced_resources) const
{
    visual_context_tree.for_each_effects_filter_bytes([&](ReadonlyBytes filter_bytes) {
        Gfx::for_each_filter_image_frame_id(filter_bytes, [&](u64 image_id) {
            referenced_resources.image_frames.set(ImageFrameResourceId { image_id }, AK::HashSetExistingEntryBehavior::Keep);
        });
    });
}

DisplayListResourceSet DisplayListResourceStorage::collect_referenced_resources(DisplayList const& display_list) const
{
    DisplayListResourceSet referenced_resources;
    collect_referenced_resources(display_list, referenced_resources);
    return referenced_resources;
}

DisplayListResourceSet DisplayListResourceStorage::collect_referenced_resources(AccumulatedVisualContextTree const& visual_context_tree) const
{
    DisplayListResourceSet referenced_resources;
    collect_referenced_resources(visual_context_tree, referenced_resources);
    return referenced_resources;
}

DisplayListResourceTransaction DisplayListResourceStorage::create_transaction(
    DisplayListResourceSet const& previous,
    DisplayListResourceSet const& current) const
{
    DisplayListResourceTransaction transaction;

    for (auto id : current.fonts) {
        if (!previous.fonts.contains(id))
            transaction.fonts.append({ id, font(id) });
    }
    for (auto id : current.image_frames) {
        if (!previous.image_frames.contains(id))
            transaction.image_frames.append({ id, image_frame(id) });
    }
    for (auto id : current.video_sinks) {
        if (previous.video_sinks.contains(id))
            continue;
        if (auto sink_handle = video_sink_handle(id); sink_handle.has_value())
            transaction.video_sinks.append({ id, *sink_handle });
    }
    for (auto id : current.display_lists) {
        if (!previous.display_lists.contains(id))
            transaction.display_lists.append({ display_list_resource(id).display_list, display_list_visual_context_tree(id) });
    }

    for (auto id : previous.fonts) {
        if (!current.fonts.contains(id))
            transaction.font_ids_to_remove.append(id);
    }
    for (auto id : previous.image_frames) {
        if (!current.image_frames.contains(id))
            transaction.image_frame_ids_to_remove.append(id);
    }
    for (auto id : previous.video_sinks) {
        if (!current.video_sinks.contains(id))
            transaction.video_sink_ids_to_remove.append(id);
    }
    for (auto id : previous.display_lists) {
        if (!current.display_lists.contains(id))
            transaction.display_list_ids_to_remove.append(id);
    }
    return transaction;
}

void DisplayListResourceStorage::apply_transaction(DisplayListResourceTransaction&& transaction)
{
    m_has_resources_added_since_last_retain = true;
    for (auto& font : transaction.fonts)
        set_font(font.id, move(font.font));
    for (auto& frame : transaction.image_frames)
        set_image_frame(frame.id, move(frame.frame));
    for (auto& video_sink : transaction.video_sinks)
        add_video_sink(video_sink.id, video_sink.sink_handle);
    for (auto& display_list : transaction.display_lists)
        add_display_list(move(display_list));

    for (auto id : transaction.font_ids_to_remove)
        m_fonts.remove(id.value());
    for (auto id : transaction.image_frame_ids_to_remove)
        m_image_frames.remove(id.value());
    for (auto id : transaction.video_sink_ids_to_remove) {
        m_video_sink_handles.remove(id.value());
        m_video_sinks.remove(id.value());
    }
    for (auto id : transaction.display_list_ids_to_remove)
        m_display_lists.remove(id.value());
}

void DisplayListResourceStorage::retain_only(DisplayListResourceSet const& resource_set)
{
    m_fonts.remove_all_matching([&](auto id, auto const&) {
        return !resource_set.fonts.contains(FontResourceId { id });
    });
    m_image_frames.remove_all_matching([&](auto id, auto const&) {
        return !resource_set.image_frames.contains(ImageFrameResourceId { id });
    });
    auto should_remove_video_resource = [&](auto id) {
        return !resource_set.video_sinks.contains(VideoSinkResourceId { id });
    };
    m_video_sink_handles.remove_all_matching([&](auto id, auto const&) { return should_remove_video_resource(id); });
    m_video_sinks.remove_all_matching([&](auto id, auto const&) { return should_remove_video_resource(id); });
    m_display_lists.remove_all_matching([&](auto id, auto const&) {
        return !resource_set.display_lists.contains(DisplayListResourceId { id });
    });
    m_has_resources_added_since_last_retain = false;
}

void DisplayListResourceStorage::set_video_sink(VideoSinkResourceId id, RefPtr<Media::VideoSink> sink)
{
    m_has_resources_added_since_last_retain = true;
    m_video_sinks.ensure(id.value(), [] { return make<DisplayListStoredVideoSinkResource>(); })->sink = move(sink);
}

// luma weights (red, blue) of a matrix the shader converts; 0 for none
static Optional<Array<float, 2>> luma_weights(Media::MatrixCoefficients matrix)
{
    switch (matrix) {
    case Media::MatrixCoefficients::BT709:
    case Media::MatrixCoefficients::Unspecified:
        return Array<float, 2> { 0.2126f, 0.0722f };
    case Media::MatrixCoefficients::BT470BG:
    case Media::MatrixCoefficients::BT601:
        return Array<float, 2> { 0.299f, 0.114f };
    case Media::MatrixCoefficients::SMPTE240:
        return Array<float, 2> { 0.212f, 0.087f };
    case Media::MatrixCoefficients::BT2020NonConstantLuminance:
        return Array<float, 2> { 0.2627f, 0.0593f };
    case Media::MatrixCoefficients::FCC:
        return Array<float, 2> { 0.30f, 0.11f };
    default:
        return {};
    }
}

// rows turning sampled y, u, v (0..1) into r, g, b: 3 x (ky, ku, kv, add)
static void yuv_rows(float kr, float kb, bool full_range, float rows[12])
{
    auto kg = 1.0f - kr - kb;
    auto ys = full_range ? 1.0f : 255.0f / 219.0f;
    auto yo = full_range ? 0.0f : -16.0f / 219.0f;
    auto cs = full_range ? 255.0f / 255.0f : 255.0f / 224.0f;
    auto co = full_range ? -128.0f / 255.0f : -128.0f / 224.0f;
    auto rv = 2.0f * (1.0f - kr);
    auto bu = 2.0f * (1.0f - kb);
    auto gu = -2.0f * kb * (1.0f - kb) / kg;
    auto gv = -2.0f * kr * (1.0f - kr) / kg;
    float const r[12] = {
        ys, 0, rv * cs, yo + rv * co,
        ys, gu * cs, gv * cs, yo + (gu + gv) * co,
        ys, bu * cs, 0, yo + bu * co,
    };
    for (int i = 0; i < 12; ++i)
        rows[i] = r[i];
}

VideoTextures DisplayListResourceStorage::webgfx_video_for_sink(VideoSinkResourceId id) const
{
    auto stored = m_video_sinks.find(id.value());
    if (stored == m_video_sinks.end() || !stored->value->sink)
        return {};
    auto& resolved = *stored->value;
    auto frame = resolved.sink->current_frame();
    if (!frame)
        return resolved.video;
    auto handle = Media::VideoFrameHandle::for_frame(*frame);
    bool have = resolved.video.rgba != 0 || resolved.video.y != 0;
    if (have && resolved.pool_id == handle.pool_id && resolved.slot_index == handle.slot_index
        && resolved.slot_acquisition_id == handle.slot_acquisition_id)
        return resolved.video;
    auto yuv_data = frame->yuv_data();
    if (!yuv_data.has_value())
        return resolved.video;
    auto size = yuv_data->size();
    auto weights = luma_weights(yuv_data->cicp().matrix_coefficients());
    bool planar = yuv_data->bit_depth() == 8 && weights.has_value();
    if (planar) {
        // three one-channel planes; a trinity shader converts them
        auto chroma = yuv_data->subsampling().subsampled_size(size);
        if (!resolved.planar || resolved.video.size != size || resolved.chroma_size != chroma) {
            resolved.free_textures();
            resolved.video.y = webgfx_plane_new(size.width(), size.height());
            resolved.video.u = webgfx_plane_new(chroma.width(), chroma.height());
            resolved.video.v = webgfx_plane_new(chroma.width(), chroma.height());
            resolved.video.size = size;
            resolved.chroma_size = chroma;
            resolved.planar = true;
        }
        yuv_rows((*weights)[0], (*weights)[1], yuv_data->cicp().video_full_range_flag() == Media::VideoFullRangeFlag::Full, resolved.video.rows);
        if (!frame->revalidate_backing())
            return resolved.video;
        webgfx_plane_update(resolved.video.y, yuv_data->y_data().data(), size.width(), size.height());
        webgfx_plane_update(resolved.video.u, yuv_data->u_data().data(), chroma.width(), chroma.height());
        webgfx_plane_update(resolved.video.v, yuv_data->v_data().data(), chroma.width(), chroma.height());
    } else {
        // deep or rare formats convert on the cpu
        auto bitmap = yuv_data->to_bitmap();
        if (bitmap.is_error()) {
            dbgln("Could not convert video frame to bitmap: {}", bitmap.error());
            return resolved.video;
        }
        if (!frame->revalidate_backing())
            return resolved.video;
        auto rgba = Gfx::rgba_from_bitmap(*bitmap.value());
        if (resolved.planar || resolved.video.rgba == 0 || resolved.video.size != size) {
            resolved.free_textures();
            resolved.video.rgba = webgfx_image_new(rgba.data(), size.width(), size.height());
            resolved.video.size = size;
            resolved.planar = false;
        } else {
            webgfx_image_update(resolved.video.rgba, rgba.data(), size.width(), size.height());
        }
    }
    resolved.pool_id = handle.pool_id;
    resolved.slot_index = handle.slot_index;
    resolved.slot_acquisition_id = handle.slot_acquisition_id;
    return resolved.video;
}

RefPtr<Media::VideoSink const> DisplayListResourceStorage::video_sink(VideoSinkResourceId id) const
{
    auto stored = m_video_sinks.find(id.value());
    if (stored == m_video_sinks.end())
        return nullptr;
    return stored->value->sink;
}

}
