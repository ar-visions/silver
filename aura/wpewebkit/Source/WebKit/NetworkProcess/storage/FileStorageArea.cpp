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

#include "config.h"
#include "FileStorageArea.h"

#include <wtf/FileSystem.h>
#include <wtf/HexNumber.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FileStorageArea);

// changes within this window are written once
constexpr Seconds saveDelay { 500_ms };

static void appendEscaped(StringBuilder& out, StringView text)
{
    unsigned length = text.length();
    for (unsigned i = 0; i < length; ++i) {
        char16_t c = text[i];
        if (c == '\\')
            out.append("\\\\"_s);
        else if (c == '\t')
            out.append("\\t"_s);
        else if (c == '\n')
            out.append("\\n"_s);
        else if (c == '\r')
            out.append("\\r"_s);
        else if (U16_IS_LEAD(c) && i + 1 < length && U16_IS_TRAIL(text[i + 1])) {
            out.append(c, text[i + 1]);
            ++i;
        } else if (c < 0x20 || U16_IS_SURROGATE(c))
            out.append("\\u"_s, hex(c, 4));
        else
            out.append(c);
    }
}

static String unescape(StringView text)
{
    StringBuilder out;
    unsigned length = text.length();
    for (unsigned i = 0; i < length; ++i) {
        char16_t c = text[i];
        if (c != '\\' || i + 1 >= length) {
            out.append(c);
            continue;
        }
        char16_t e = text[++i];
        if (e == 't')
            out.append('\t');
        else if (e == 'n')
            out.append('\n');
        else if (e == 'r')
            out.append('\r');
        else if (e == 'u' && i + 4 < length) {
            auto code = parseInteger<uint16_t>(text.substring(i + 1, 4), 16);
            out.append(static_cast<char16_t>(code.value_or(replacementCharacter)));
            i += 4;
        } else
            out.append(e);
    }
    return out.toString();
}

Ref<FileStorageArea> FileStorageArea::create(unsigned quota, const WebCore::ClientOrigin& origin, const String& path, Ref<WorkQueue>&& workQueue)
{
    return adoptRef(*new FileStorageArea(quota, origin, path, WTF::move(workQueue)));
}

FileStorageArea::FileStorageArea(unsigned quota, const WebCore::ClientOrigin& origin, const String& path, Ref<WorkQueue>&& workQueue)
    : StorageAreaBase(quota, origin)
    , m_path(path)
    , m_queue(WTF::move(workQueue))
    , m_map(quota)
{
    assertIsCurrent(m_queue.get());
    load();
}

FileStorageArea::~FileStorageArea()
{
    assertIsCurrent(m_queue.get());
    close();
}

void FileStorageArea::load()
{
    auto bytes = FileSystem::readEntireFile(m_path);
    if (!bytes)
        return;

    HashMap<String, String> items;
    auto text = String::fromUTF8(bytes->span());
    for (auto line : StringView(text).split('\n')) {
        auto tab = line.find('\t');
        if (tab == notFound)
            continue;
        items.set(unescape(line.left(tab)), unescape(line.substring(tab + 1)));
    }
    m_map.importItems(WTF::move(items));
}

void FileStorageArea::save()
{
    assertIsCurrent(m_queue.get());

    m_saveScheduled = false;
    if (!m_dirty)
        return;
    m_dirty = false;

    if (!m_map.length()) {
        FileSystem::deleteFile(m_path);
        return;
    }

    StringBuilder out;
    for (auto& [key, value] : m_map.items()) {
        appendEscaped(out, key);
        out.append('\t');
        appendEscaped(out, value);
        out.append('\n');
    }
    FileSystem::makeAllDirectories(FileSystem::parentPath(m_path));
    auto utf8 = out.toString().utf8();
    FileSystem::overwriteEntireFile(m_path, byteCast<uint8_t>(utf8.span()));
}

void FileStorageArea::scheduleSave()
{
    m_dirty = true;
    if (m_saveScheduled)
        return;
    m_saveScheduled = true;

    m_queue->dispatchAfter(saveDelay, [weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->save();
    });
}

void FileStorageArea::close()
{
    save();
}

bool FileStorageArea::isEmpty()
{
    return !m_map.length();
}

void FileStorageArea::clear()
{
    m_map.clear();
    m_dirty = true;
    save();
    notifyListenersAboutClear();
}

HashMap<String, String> FileStorageArea::allItems()
{
    return m_map.items();
}

Expected<void, StorageError> FileStorageArea::setItem(std::optional<IPC::Connection::UniqueID> connection, std::optional<StorageAreaImplIdentifier> storageAreaImplID, String&& key, String&& value, const String& urlString)
{
    String oldValue;
    bool hasQuotaError = false;
    m_map.setItem(key, value, oldValue, hasQuotaError);
    if (hasQuotaError)
        return makeUnexpected(StorageError::QuotaExceeded);

    scheduleSave();
    if (connection && storageAreaImplID)
        dispatchEvents(*connection, *storageAreaImplID, key, oldValue, value, urlString);

    return { };
}

Expected<void, StorageError> FileStorageArea::removeItem(IPC::Connection::UniqueID connection, StorageAreaImplIdentifier storageAreaImplID, const String& key, const String& urlString)
{
    String oldValue;
    m_map.removeItem(key, oldValue);
    if (oldValue.isNull())
        return makeUnexpected(StorageError::ItemNotFound);

    scheduleSave();
    dispatchEvents(connection, storageAreaImplID, key, oldValue, String(), urlString);

    return { };
}

Expected<void, StorageError> FileStorageArea::clear(IPC::Connection::UniqueID connection, StorageAreaImplIdentifier storageAreaImplID, const String& urlString)
{
    if (!m_map.length())
        return { };

    m_map.clear();
    scheduleSave();
    dispatchEvents(connection, storageAreaImplID, String(), String(), String(), urlString);

    return { };
}

} // namespace WebKit
