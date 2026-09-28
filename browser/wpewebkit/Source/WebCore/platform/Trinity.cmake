list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/harfbuzz"
    "${WEBCORE_DIR}/platform/graphics/trinity"
    "${WEBCORE_DIR}/platform/graphics/filters/trinity"
)

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "platform/SourcesTrinity.txt"
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/graphics/harfbuzz/HbUniquePtr.h
    platform/graphics/trinity/FontCascadeTrinityInlines.h
    platform/graphics/trinity/FontVariationsTrinity.h
    platform/graphics/trinity/GraphicsContextTrinity.h
    platform/graphics/trinity/ImageBufferTrinityBackend.h
    platform/graphics/trinity/PathTrinity.h
    platform/graphics/trinity/TrinityImage.h
    platform/graphics/trinity/TrinityPNG.h
    platform/graphics/trinity/TrinityPaintingEngine.h
    platform/graphics/trinity/TrinityPattern.h
    platform/graphics/trinity/TrinityTypeface.h
    platform/graphics/trinity/WebGfx.h
)

list(APPEND WebCore_LIBRARIES
    HarfBuzz::HarfBuzz
    HarfBuzz::ICU
    webgfx
)
