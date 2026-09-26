include_guard()

find_package(PkgConfig REQUIRED)

if (NOT WIN32 AND NOT ANDROID)
    pkg_check_modules(LIBEDIT REQUIRED IMPORTED_TARGET libedit)
endif()

find_package(mimalloc CONFIG REQUIRED)

# AK dependencies
find_package(Backtrace)
find_package(cpptrace CONFIG)
find_package(FastFloat CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(simdutf REQUIRED)

# LibGfx dependencies
find_package(harfbuzz REQUIRED)
find_package(JPEG REQUIRED)
find_package(LIBAVIF REQUIRED)
find_package(PNG REQUIRED)
find_package(WebP REQUIRED)
find_path(WUFFS_INCLUDE_DIR NAMES wuffs/wuffs-v0.3.c REQUIRED)

pkg_check_modules(WOFF2 REQUIRED IMPORTED_TARGET libwoff2dec)

# TODO: Figure out if we can do this the same way on all platforms
if (NOT ANDROID)
    pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec-ladybird)
    pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat-ladybird)
    pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil-ladybird)
    pkg_check_modules(LIBSWRESAMPLE REQUIRED IMPORTED_TARGET libswresample-ladybird)
else()
    find_package(FFMPEG REQUIRED)
endif()

if (NOT APPLE AND NOT ANDROID AND NOT WIN32)
    find_package(Fontconfig REQUIRED)
    set(HAS_FONTCONFIG ON CACHE BOOL "" FORCE)
    add_cxx_compile_definitions(USE_FONTCONFIG=1)
endif()

# trinity owns the gpu
set(HAS_VULKAN OFF CACHE BOOL "" FORCE)
set(USE_VULKAN_DMABUF_IMAGES OFF CACHE BOOL "" FORCE)

find_package(CURL REQUIRED)
find_package(ICU 78.3 EXACT REQUIRED COMPONENTS data i18n uc)
find_package(LibXml2 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(simdjson CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

find_package(unofficial-brotli CONFIG)
if(unofficial-brotli_FOUND)
    set(BROTLI_TARGETS unofficial::brotli::brotlienc unofficial::brotli::brotlidec)
else()
    pkg_check_modules(BROTLI REQUIRED IMPORTED_TARGET libbrotlienc libbrotlidec libbrotlicommon)
    set(BROTLI_TARGETS PkgConfig::BROTLI)
endif()

pkg_check_modules(LIBPSL REQUIRED IMPORTED_TARGET libpsl)
pkg_check_modules(libtommath REQUIRED IMPORTED_TARGET libtommath)

# trinity draws (silver webgfx); no ANGLE library
set(ANGLE_TARGETS)
# WebGL's GL constants and types: ANGLE's headers, nothing built
set(GLES_HEADERS_DIR ${CMAKE_BINARY_DIR}/gles-headers)
foreach(header GLES2/gl2.h GLES2/gl2ext.h GLES2/gl2ext_angle.h GLES2/gl2platform.h GLES3/gl3.h GLES3/gl3platform.h KHR/khrplatform.h)
    if (NOT EXISTS ${GLES_HEADERS_DIR}/${header})
        file(DOWNLOAD https://raw.githubusercontent.com/google/angle/chromium/7258/include/${header} ${GLES_HEADERS_DIR}/${header} STATUS download_status)
        list(GET download_status 0 download_code)
        if (NOT download_code EQUAL 0)
            message(FATAL_ERROR "Could not fetch ANGLE header ${header}")
        endif()
    endif()
endforeach()
include_directories(SYSTEM ${GLES_HEADERS_DIR})

if (WIN32)
    find_package(pthread REQUIRED)
    find_package(mman REQUIRED)
endif()

find_package(Git REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# trinity draws: silver's webgfx takes skia's place
find_library(WEBGFX_LIBRARY silver-webgfx HINTS ${SILVER_INSTALL}/build REQUIRED)
add_library(webgfx SHARED IMPORTED)
set_target_properties(webgfx PROPERTIES IMPORTED_LOCATION ${WEBGFX_LIBRARY})
set(HAS_DIRECTX OFF CACHE BOOL "" FORCE)
