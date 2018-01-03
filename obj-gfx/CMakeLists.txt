file(GLOB src "*.c")
add_library (obj-gfx SHARED ${src})

find_package(Freetype REQUIRED)
include_directories(${FREETYPE_INCLUDE_DIRS})

if(CPP)
    set_source_files_properties (${src} PROPERTIES LANGUAGE CXX)
endif()

if(MSVC)
    add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
endif()

set_property(TARGET obj-gfx PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries (obj-gfx LINK_PUBLIC obj-math m glfw GLESv2 freetype2)
