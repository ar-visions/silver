// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "TrinityTypeface.h"

#if USE(TRINITY)

#include "WebGfx.h"
#include <wtf/FileSystem.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

RefPtr<TrinityTypeface> TrinityTypeface::create(Vector<uint8_t>&& bytes, unsigned index, Vector<Axis>&& axes)
{
    if (bytes.isEmpty())
        return nullptr;
    // FreeType and HarfBuzz both read the bytes in place
    auto font = webgfx_font_open(bytes.span().data(), static_cast<int64_t>(bytes.size()), static_cast<int>(index));
    if (!font)
        return nullptr;
    auto* blob = hb_blob_create(reinterpret_cast<const char*>(bytes.span().data()), bytes.size(), HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    auto* face = hb_face_create(blob, index);
    hb_blob_destroy(blob);
    return adoptRef(*new TrinityTypeface(WTF::move(bytes), index, font, face, WTF::move(axes)));
}

RefPtr<TrinityTypeface> TrinityTypeface::createFromFile(const String& path, unsigned index)
{
    auto contents = FileSystem::readEntireFile(path);
    if (!contents)
        return nullptr;
    return create(WTF::move(*contents), index);
}

// one typeface per file and index, shared by every match
RefPtr<TrinityTypeface> TrinityTypeface::matchSystem(const String& family, int weight, int width, int slope, char32_t character, bool emoji)
{
    static Lock lock;
    static NeverDestroyed<HashMap<String, RefPtr<TrinityTypeface>>> files;
    uint8_t file[1024];
    uint8_t matched[256];
    int index = 0;
    auto familyUTF8 = family.utf8();
    if (!webgfx_font_match(family.isEmpty() ? nullptr : familyUTF8.data(), weight, width, slope, character, emoji, file, sizeof(file), matched, sizeof(matched), &index))
        return nullptr;
    auto path = String::fromUTF8(reinterpret_cast<const char*>(file));
    auto key = makeString(path, '#', index);
    Locker locker { lock };
    auto result = files->ensure(key, [&] {
        return createFromFile(path, index);
    });
    return result.iterator->value;
}

TrinityTypeface::TrinityTypeface(Vector<uint8_t>&& bytes, unsigned index, int font, hb_face_t* face, Vector<Axis>&& axes)
    : m_bytes(WTF::move(bytes))
    , m_index(index)
    , m_font(font)
    , m_hbFace(face)
    , m_axes(WTF::move(axes))
{
    if (!m_axes.isEmpty()) {
        Vector<uint32_t> tags;
        Vector<float> values;
        for (auto& axis : m_axes) {
            tags.append(axis.tag);
            values.append(axis.value);
        }
        webgfx_font_set_axes(m_font, tags.span().data(), values.span().data(), static_cast<int>(tags.size()));
    }
    uint8_t name[256];
    auto length = webgfx_font_family(m_font, name, sizeof(name));
    if (length > 0)
        m_familyName = String::fromUTF8(std::span { name, static_cast<size_t>(length) });
    webgfx_font_style(m_font, m_style);
}

TrinityTypeface::~TrinityTypeface()
{
    hb_face_destroy(m_hbFace);
    webgfx_font_close(m_font);
}

RefPtr<TrinityTypeface> TrinityTypeface::withAxes(Vector<Axis>&& axes) const
{
    auto bytes = m_bytes;
    return create(WTF::move(bytes), m_index, WTF::move(axes));
}

unsigned TrinityTypeface::unitsPerEm() const
{
    return static_cast<unsigned>(webgfx_font_units_per_em(m_font));
}

unsigned TrinityTypeface::glyphCount() const
{
    return static_cast<unsigned>(webgfx_font_glyph_count(m_font));
}

uint32_t TrinityTypeface::glyphForCharacter(char32_t character) const
{
    return webgfx_font_glyph_index(m_font, character);
}

} // namespace WebCore

#endif // USE(TRINITY)
