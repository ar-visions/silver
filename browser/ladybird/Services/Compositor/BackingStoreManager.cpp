/*
 * Copyright (c) 2024-2026, Aliaksandr Kalenik <kalenik.aliaksandr@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/AnyOf.h>
#include <Compositor/BackingStoreManager.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/PaintingSurface.h>
#include <LibGfx/SharedImageBuffer.h>

namespace Compositor {

Optional<BackingStoreManager::Allocation> BackingStoreManager::resize_backing_stores_if_needed(
    Gfx::IntSize viewport_size, Compositing::WindowResizingInProgress window_resize_in_progress)
{
    if (viewport_size.is_empty())
        return {};

    auto minimum_needed_size = viewport_size;
    bool force_reallocate = false;
    if (window_resize_in_progress == Compositing::WindowResizingInProgress::Yes) {
        // Pad the minimum needed size so that we don't have to keep reallocating backing stores while the window is being resized.
        minimum_needed_size = { viewport_size.width() + 256, viewport_size.height() + 256 };
    } else {
        // If we're not in the middle of a resize, we can shrink the backing store size to match the viewport size.
        minimum_needed_size = viewport_size;
        force_reallocate = m_allocated_size != minimum_needed_size;
    }

    if (force_reallocate || m_allocated_size.is_empty() || !m_allocated_size.contains(minimum_needed_size)) {
        m_allocated_size = minimum_needed_size;
        Vector<i32> bitmap_ids;
        bitmap_ids.ensure_capacity(2);
        for (size_t i = 0; i < 2; ++i)
            bitmap_ids.append(m_next_bitmap_id++);
        return Allocation { .size = minimum_needed_size, .bitmap_ids = move(bitmap_ids) };
    }

    return {};
}

Optional<BackingStoreManager::Publication> BackingStoreManager::allocate_backing_stores(Allocation const& allocation, bool should_publish)
{
    m_backing_stores.clear();
    m_rendering_store_index.clear();
    m_latest_rendered_store_index.clear();

    if (Gfx::Bitmap::size_would_overflow(Gfx::BitmapFormat::BGRA8888, allocation.size))
        return {};

    auto buffer_count = allocation.bitmap_ids.size();
    m_backing_stores.ensure_capacity(buffer_count);

    if (!should_publish) {
        for (size_t i = 0; i < buffer_count; ++i) {
            m_backing_stores.append({
                .surface = Gfx::PaintingSurface::create_with_size(allocation.size, Gfx::BitmapFormat::BGRA8888, Gfx::AlphaType::Premultiplied),
                .bitmap_id = allocation.bitmap_ids[i],
                .state = BufferState::Available,
                .accumulated_damage = { {}, allocation.size },
            });
        }
        return {};
    }

    // The UI installs the first published buffer as its initial front buffer.
    // Reserve it until the UI releases it after presenting another buffer.
    auto initial_buffer_state = [](size_t index) {
        return index == 0 ? BufferState::Presented : BufferState::Available;
    };

    Vector<Gfx::SharedImage> shared_images;
    shared_images.ensure_capacity(buffer_count);
    for (size_t i = 0; i < buffer_count; ++i) {
        auto buffer = Gfx::SharedImageBuffer::create(allocation.size);
        shared_images.append(buffer.export_shared_image());
        m_backing_stores.append({
            // a trinity canvas; each flush copies into the shared bitmap
            .surface = Gfx::PaintingSurface::wrap_bitmap(*buffer.bitmap()),
            .bitmap_id = allocation.bitmap_ids[i],
            .state = initial_buffer_state(i),
            .accumulated_damage = { {}, allocation.size },
        });
    }

    return Publication {
        .bitmap_ids = allocation.bitmap_ids,
        .shared_images = move(shared_images),
    };
}

bool BackingStoreManager::is_valid() const
{
    return !m_backing_stores.is_empty();
}

bool BackingStoreManager::has_available_buffer() const
{
    return any_of(m_backing_stores, [](auto const& store) { return store.state == BufferState::Available; });
}

Optional<BackingStoreManager::RenderTarget> BackingStoreManager::acquire_render_target(Gfx::IntRect frame_damage)
{
    VERIFY(!m_rendering_store_index.has_value());
    for (auto& store : m_backing_stores)
        store.accumulated_damage.unite(frame_damage);

    for (size_t i = 0; i < m_backing_stores.size(); ++i) {
        auto& store = m_backing_stores[i];
        if (store.state != BufferState::Available)
            continue;

        store.state = BufferState::Rendering;
        m_rendering_store_index = i;
        auto damage_rect = store.accumulated_damage;
        store.accumulated_damage = {};
        return RenderTarget { *store.surface, store.bitmap_id, damage_rect };
    }
    return {};
}

void BackingStoreManager::complete_rendering(i32 bitmap_id, bool wait_for_release)
{
    VERIFY(m_rendering_store_index.has_value());
    auto& store = m_backing_stores[*m_rendering_store_index];
    VERIFY(store.state == BufferState::Rendering);
    VERIFY(store.bitmap_id == bitmap_id);

    if (!wait_for_release && m_latest_rendered_store_index.has_value())
        m_backing_stores[*m_latest_rendered_store_index].state = BufferState::Available;

    store.state = BufferState::Presented;
    m_latest_rendered_store_index = m_rendering_store_index;
    m_rendering_store_index.clear();
}

bool BackingStoreManager::release_buffer(i32 bitmap_id)
{
    for (auto& store : m_backing_stores) {
        if (store.bitmap_id != bitmap_id || store.state != BufferState::Presented)
            continue;
        store.state = BufferState::Available;
        return true;
    }
    return false;
}

RefPtr<Gfx::PaintingSurface> BackingStoreManager::latest_rendered_surface() const
{
    if (!m_latest_rendered_store_index.has_value())
        return nullptr;
    return m_backing_stores[*m_latest_rendered_store_index].surface;
}

}
