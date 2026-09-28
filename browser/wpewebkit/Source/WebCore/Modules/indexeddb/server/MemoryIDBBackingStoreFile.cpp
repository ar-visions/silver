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
#include "MemoryIDBBackingStore.h"

#include "IDBDatabaseNameAndVersion.h"
#include "IDBIndexInfo.h"
#include "IDBKeyData.h"
#include "IndexValueStore.h"
#include "MemoryIndex.h"
#include "MemoryObjectStore.h"
#include <wtf/FileSystem.h>
#include <wtf/HexNumber.h>
#include <wtf/dtoa.h>
#include <wtf/text/Base64.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
namespace IDBServer {

static constexpr auto fileSuffix = ".indexeddb.txt"_s;

// escapes tab, newline, quote and backslash
static void appendEscaped(StringBuilder& out, StringView text)
{
    unsigned length = text.length();
    for (unsigned i = 0; i < length; ++i) {
        char16_t c = text[i];
        if (c == '\\')
            out.append("\\\\"_s);
        else if (c == '"')
            out.append("\\\""_s);
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

static void appendNumber(StringBuilder& out, double value)
{
    if (std::isinf(value))
        out.append(value < 0 ? "-inf"_s : "inf"_s);
    else
        out.append(String::number(value));
}

static void appendKey(StringBuilder& out, const IDBKeyData& key)
{
    WTF::switchOn(key.value(),
        [&](const Vector<IDBKeyData>& array) {
            out.append("a:["_s);
            bool first = true;
            for (auto& item : array) {
                if (!first)
                    out.append(',');
                first = false;
                appendKey(out, item);
            }
            out.append(']');
        },
        [&](const String& string) {
            out.append("s:\""_s);
            appendEscaped(out, string);
            out.append('"');
        },
        [&](double number) {
            out.append("n:"_s);
            appendNumber(out, number);
        },
        [&](const IDBKeyData::Date& date) {
            out.append("d:"_s);
            appendNumber(out, date.value);
        },
        [&](const ThreadSafeDataBuffer& binary) {
            out.append("b:"_s);
            if (auto* data = binary.data())
                out.append(base64EncodeToString(data->span()));
        },
        [&](const auto&) {
            out.append("-"_s);
        });
}

static void appendKeyPath(StringBuilder& out, const std::optional<IDBKeyPath>& keyPath)
{
    if (!keyPath) {
        out.append('-');
        return;
    }
    WTF::switchOn(*keyPath,
        [&](const String& path) {
            out.append("s:\""_s);
            appendEscaped(out, path);
            out.append('"');
        },
        [&](const Vector<String>& paths) {
            out.append("a:["_s);
            bool first = true;
            for (auto& path : paths) {
                if (!first)
                    out.append(',');
                first = false;
                out.append("s:\""_s);
                appendEscaped(out, path);
                out.append('"');
            }
            out.append(']');
        });
}

// reads keys, key paths and strings from a field
class FieldReader {
public:
    explicit FieldReader(StringView text)
        : m_text(text)
    {
    }

    bool atEnd() const { return m_position >= m_text.length(); }

    std::optional<String> readQuoted()
    {
        if (!consume('"'))
            return std::nullopt;
        StringBuilder out;
        while (!atEnd()) {
            char16_t c = m_text[m_position++];
            if (c == '"')
                return out.toString();
            if (c != '\\' || atEnd()) {
                out.append(c);
                continue;
            }
            char16_t e = m_text[m_position++];
            if (e == 't')
                out.append('\t');
            else if (e == 'n')
                out.append('\n');
            else if (e == 'r')
                out.append('\r');
            else if (e == 'u' && m_position + 4 <= m_text.length()) {
                auto code = parseInteger<uint16_t>(m_text.substring(m_position, 4), 16);
                out.append(static_cast<char16_t>(code.value_or(replacementCharacter)));
                m_position += 4;
            } else
                out.append(e);
        }
        return std::nullopt;
    }

    std::optional<double> readNumber()
    {
        auto rest = m_text.substring(m_position);
        if (rest.startsWith("inf"_s)) {
            m_position += 3;
            return std::numeric_limits<double>::infinity();
        }
        if (rest.startsWith("-inf"_s)) {
            m_position += 4;
            return -std::numeric_limits<double>::infinity();
        }
        size_t parsed = 0;
        double value = parseDouble(rest, parsed);
        if (!parsed)
            return std::nullopt;
        m_position += parsed;
        return value;
    }

    std::optional<IDBKeyData> readKey()
    {
        auto rest = m_text.substring(m_position);
        if (rest.length() < 2 || rest[1] != ':')
            return std::nullopt;
        char16_t type = rest[0];
        m_position += 2;

        IDBKeyData key;
        if (type == 'n') {
            auto number = readNumber();
            if (!number)
                return std::nullopt;
            key.setNumberValue(*number);
        } else if (type == 'd') {
            auto number = readNumber();
            if (!number)
                return std::nullopt;
            key.setDateValue(*number);
        } else if (type == 's') {
            auto string = readQuoted();
            if (!string)
                return std::nullopt;
            key.setStringValue(*string);
        } else if (type == 'b') {
            unsigned start = m_position;
            while (!atEnd() && m_text[m_position] != ',' && m_text[m_position] != ']')
                ++m_position;
            auto bytes = base64Decode(m_text.substring(start, m_position - start));
            if (!bytes)
                return std::nullopt;
            key.setBinaryValue(ThreadSafeDataBuffer::create(WTF::move(*bytes)));
        } else if (type == 'a') {
            if (!consume('['))
                return std::nullopt;
            Vector<IDBKeyData> items;
            while (!consume(']')) {
                if (!items.isEmpty() && !consume(','))
                    return std::nullopt;
                auto item = readKey();
                if (!item)
                    return std::nullopt;
                items.append(WTF::move(*item));
            }
            key.setArrayValue(items);
        } else
            return std::nullopt;
        return key;
    }

    std::optional<std::optional<IDBKeyPath>> readKeyPath()
    {
        if (consume('-'))
            return std::optional<IDBKeyPath> { };
        auto rest = m_text.substring(m_position);
        if (rest.startsWith("s:"_s)) {
            m_position += 2;
            auto path = readQuoted();
            if (!path)
                return std::nullopt;
            return std::optional<IDBKeyPath> { IDBKeyPath { *path } };
        }
        if (!rest.startsWith("a:["_s))
            return std::nullopt;
        m_position += 3;
        Vector<String> paths;
        while (!consume(']')) {
            if (!paths.isEmpty() && !consume(','))
                return std::nullopt;
            if (!consume('s') || !consume(':'))
                return std::nullopt;
            auto path = readQuoted();
            if (!path)
                return std::nullopt;
            paths.append(WTF::move(*path));
        }
        return std::optional<IDBKeyPath> { IDBKeyPath { WTF::move(paths) } };
    }

private:
    bool consume(char16_t c)
    {
        if (atEnd() || m_text[m_position] != c)
            return false;
        ++m_position;
        return true;
    }

    StringView m_text;
    unsigned m_position { 0 };
};

static std::optional<IDBKeyData> keyFromField(StringView field)
{
    FieldReader reader(field);
    auto key = reader.readKey();
    if (!key || !reader.atEnd())
        return std::nullopt;
    return key;
}

static std::optional<std::optional<IDBKeyPath>> keyPathFromField(StringView field)
{
    FieldReader reader(field);
    auto keyPath = reader.readKeyPath();
    if (!keyPath || !reader.atEnd())
        return std::nullopt;
    return keyPath;
}

static std::optional<String> nameFromField(StringView field)
{
    StringBuilder quoted;
    quoted.append('"', field, '"');
    auto text = quoted.toString();
    FieldReader reader(text);
    return reader.readQuoted();
}

static std::optional<uint64_t> integerFromField(StringView field)
{
    return parseInteger<uint64_t>(field);
}

static Vector<StringView> fieldsOf(StringView line)
{
    Vector<StringView> fields;
    for (auto field : line.splitAllowingEmptyEntries('\t'))
        fields.append(field);
    return fields;
}

String MemoryIDBBackingStore::fileNameForDatabase(const String& databaseName)
{
    return makeString(FileSystem::encodeForFileName(databaseName), fileSuffix);
}

bool MemoryIDBBackingStore::isDatabaseFileName(const String& fileName)
{
    return fileName.endsWith(fileSuffix);
}

std::optional<IDBDatabaseNameAndVersion> MemoryIDBBackingStore::databaseNameAndVersionFromFile(const String& path)
{
    auto bytes = FileSystem::readEntireFile(path);
    if (!bytes)
        return std::nullopt;
    auto text = String::fromUTF8(bytes->span());
    auto end = text.find('\n');
    auto fields = fieldsOf(StringView(text).left(end == notFound ? text.length() : end));
    if (fields.size() != 3 || fields[0] != "database"_s)
        return std::nullopt;
    auto name = nameFromField(fields[1]);
    auto version = integerFromField(fields[2]);
    if (!name || !version)
        return std::nullopt;
    return IDBDatabaseNameAndVersion { *name, *version };
}

void MemoryIDBBackingStore::saveFile()
{
    ASSERT(m_databaseInfo);

    StringBuilder out;
    out.append("database\t"_s);
    appendEscaped(out, m_databaseInfo->name());
    out.append('\t', m_databaseInfo->version(), '\n');

    for (auto& objectStore : m_objectStoresByIdentifier.values()) {
        auto& info = objectStore->info();
        out.append("store\t"_s, info.identifier().toRawValue(), '\t');
        appendEscaped(out, info.name());
        out.append('\t');
        appendKeyPath(out, info.keyPath());
        out.append('\t', info.autoIncrement() ? '1' : '0', '\t', objectStore->currentKeyGeneratorValue(), '\n');

        for (auto& indexInfo : info.indexMap().values()) {
            out.append("index\t"_s, info.identifier().toRawValue(), '\t', indexInfo.identifier().toRawValue(), '\t');
            appendEscaped(out, indexInfo.name());
            out.append('\t');
            appendKeyPath(out, std::optional<IDBKeyPath> { indexInfo.keyPath() });
            out.append('\t', indexInfo.unique() ? '1' : '0', '\t', indexInfo.multiEntry() ? '1' : '0', '\n');
        }
    }

    for (auto& objectStore : m_objectStoresByIdentifier.values()) {
        auto storeID = objectStore->info().identifier().toRawValue();
        objectStore->forEachRecord([&](const IDBKeyData& key, const IDBValue& value) {
            out.append("record\t"_s, storeID, '\t');
            appendKey(out, key);
            out.append('\t');
            if (auto* data = value.data().data())
                out.append(base64EncodeToString(data->span()));
            out.append('\n');
        });

        for (auto& indexInfo : objectStore->info().indexMap().values()) {
            RefPtr index = objectStore->indexForIdentifier(indexInfo.identifier());
            CheckedPtr values = index ? index->valueStore() : nullptr;
            if (!values)
                continue;
            for (auto& indexKey : values->allKeys()) {
                auto primaryKeys = values->valueKeys(indexKey);
                if (!primaryKeys)
                    continue;
                for (auto& primaryKey : *primaryKeys) {
                    out.append("entry\t"_s, storeID, '\t', indexInfo.identifier().toRawValue(), '\t');
                    appendKey(out, indexKey);
                    out.append('\t');
                    appendKey(out, primaryKey);
                    out.append('\n');
                }
            }
        }
    }

    FileSystem::makeAllDirectories(FileSystem::parentPath(m_filePath));
    auto utf8 = out.toString().utf8();
    FileSystem::overwriteEntireFile(m_filePath, byteCast<uint8_t>(utf8.span()));
}

void MemoryIDBBackingStore::loadFile()
{
    auto bytes = FileSystem::readEntireFile(m_filePath);
    if (!bytes)
        return;

    auto text = String::fromUTF8(bytes->span());
    Vector<Vector<StringView>> lines;
    for (auto line : StringView(text).split('\n'))
        lines.append(fieldsOf(line));

    if (lines.isEmpty() || lines[0].size() != 3 || lines[0][0] != "database"_s)
        return;
    auto name = nameFromField(lines[0][1]);
    auto version = integerFromField(lines[0][2]);
    if (!name || !version)
        return;

    struct StoreLine {
        IDBObjectStoreInfo info;
        uint64_t keyGenerator;
    };
    Vector<StoreLine> stores;
    uint64_t maxIndexID = 0;

    for (auto& fields : lines) {
        if (fields.size() == 6 && fields[0] == "store"_s) {
            auto id = integerFromField(fields[1]);
            auto storeName = nameFromField(fields[2]);
            auto keyPath = keyPathFromField(fields[3]);
            auto keyGenerator = integerFromField(fields[5]);
            if (!id || !storeName || !keyPath || !keyGenerator)
                continue;
            stores.append({ IDBObjectStoreInfo { IDBObjectStoreIdentifier { *id }, *storeName, WTF::move(*keyPath), fields[4] == "1"_s }, *keyGenerator });
        } else if (fields.size() == 7 && fields[0] == "index"_s) {
            auto storeID = integerFromField(fields[1]);
            auto id = integerFromField(fields[2]);
            auto indexName = nameFromField(fields[3]);
            auto keyPath = keyPathFromField(fields[4]);
            if (!storeID || !id || !indexName || !keyPath || !*keyPath)
                continue;
            for (auto& store : stores) {
                if (store.info.identifier().toRawValue() != *storeID)
                    continue;
                store.info.addExistingIndex(IDBIndexInfo { IDBIndexIdentifier { *id }, store.info.identifier(), *indexName, WTF::move(**keyPath), fields[5] == "1"_s, fields[6] == "1"_s });
                maxIndexID = std::max(maxIndexID, *id);
            }
        }
    }

    m_databaseInfo = makeUnique<IDBDatabaseInfo>(*name, *version, maxIndexID);
    for (auto& store : stores) {
        m_databaseInfo->addExistingObjectStore(store.info);
        auto objectStore = MemoryObjectStore::create(store.info);
        objectStore->setKeyGeneratorValue(store.keyGenerator);
        for (auto& indexInfo : store.info.indexMap().values())
            objectStore->registerIndex(MemoryIndex::create(indexInfo, objectStore.get()));
        registerObjectStore(WTF::move(objectStore));
    }

    for (auto& fields : lines) {
        if (fields.size() == 4 && fields[0] == "record"_s) {
            auto storeID = integerFromField(fields[1]);
            auto key = keyFromField(fields[2]);
            auto value = base64Decode(fields[3]);
            if (!storeID || !key || !value)
                continue;
            if (RefPtr objectStore = m_objectStoresByIdentifier.get(IDBObjectStoreIdentifier { *storeID }))
                objectStore->loadRecord(*key, ThreadSafeDataBuffer::create(WTF::move(*value)));
        } else if (fields.size() == 5 && fields[0] == "entry"_s) {
            auto storeID = integerFromField(fields[1]);
            auto indexID = integerFromField(fields[2]);
            auto indexKey = keyFromField(fields[3]);
            auto primaryKey = keyFromField(fields[4]);
            if (!storeID || !indexID || !indexKey || !primaryKey)
                continue;
            RefPtr objectStore = m_objectStoresByIdentifier.get(IDBObjectStoreIdentifier { *storeID });
            RefPtr index = objectStore ? objectStore->indexForIdentifier(IDBIndexIdentifier { *indexID }) : nullptr;
            if (CheckedPtr values = index ? index->valueStore() : nullptr)
                values->addRecord(*indexKey, *primaryKey);
        }
    }
}

} // namespace IDBServer
} // namespace WebCore
