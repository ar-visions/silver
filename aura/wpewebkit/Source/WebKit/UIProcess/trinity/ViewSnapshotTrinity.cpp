// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "ViewSnapshotStore.h"

#if USE(TRINITY)

namespace WebKit {
using namespace WebCore;

Ref<ViewSnapshot> ViewSnapshot::create(Ref<TrinityImage>&& image)
{
    return adoptRef(*new ViewSnapshot(WTF::move(image)));
}

ViewSnapshot::ViewSnapshot(Ref<TrinityImage>&& image)
    : m_image(WTF::move(image))
{
    if (hasImage())
        ViewSnapshotStore::singleton().didAddImageToSnapshot(*this);
}

bool ViewSnapshot::hasImage() const
{
    return !!m_image;
}

void ViewSnapshot::clearImage()
{
    if (!hasImage())
        return;
    ViewSnapshotStore::singleton().willRemoveImageFromSnapshot(*this);
    m_image = nullptr;
}

size_t ViewSnapshot::estimatedImageSizeInBytes() const
{
    return m_image ? m_image->pixels().size() : 0;
}

WebCore::IntSize ViewSnapshot::size() const
{
    return m_image ? m_image->size() : IntSize();
}

} // namespace WebKit

#endif // USE(TRINITY)
