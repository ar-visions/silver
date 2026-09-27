/*
 * Copyright (C) 2026 Kalen White
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DAMAGES ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "StorageAreaBase.h"
#include <WebCore/StorageMap.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

// localStorage kept as a readable text file: key, tab, value per line
class FileStorageArea final : public StorageAreaBase, public RefCounted<FileStorageArea> {
    WTF_MAKE_TZONE_ALLOCATED(FileStorageArea);
public:
    static Ref<FileStorageArea> create(unsigned quota, const WebCore::ClientOrigin&, const String& path, Ref<WorkQueue>&&);
    ~FileStorageArea();

    void close();
    void save();
    void clear() final;

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    FileStorageArea(unsigned quota, const WebCore::ClientOrigin&, const String& path, Ref<WorkQueue>&&);

    // StorageAreaBase
    Type type() const final { return StorageAreaBase::Type::File; };
    StorageType storageType() const final { return StorageAreaBase::StorageType::Local; };
    bool isEmpty() final;
    HashMap<String, String> allItems() final;
    Expected<void, StorageError> setItem(std::optional<IPC::Connection::UniqueID>, std::optional<StorageAreaImplIdentifier>, String&& key, String&& value, const String& urlString) final;
    Expected<void, StorageError> removeItem(IPC::Connection::UniqueID, StorageAreaImplIdentifier, const String& key, const String& urlString) final;
    Expected<void, StorageError> clear(IPC::Connection::UniqueID, StorageAreaImplIdentifier, const String& urlString) final;

    void load();
    void scheduleSave();

    String m_path;
    const Ref<WorkQueue> m_queue;
    WebCore::StorageMap m_map;
    bool m_dirty { false };
    bool m_saveScheduled { false };
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::FileStorageArea)
    static bool isType(const WebKit::StorageAreaBase& area) { return area.type() == WebKit::StorageAreaBase::Type::File; }
SPECIALIZE_TYPE_TRAITS_END()
