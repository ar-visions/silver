file(GLOB src "*.c")
add_library (obj-gfx SHARED ${src})

if(CPP)
    set_source_files_properties (${src} PROPERTIES LANGUAGE CXX)
endif()

if(MSVC)
    add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
endif()

set_property(TARGET obj-gfx PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories (obj-gfx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries (obj-gfx LINK_PUBLIC obj-math m glfw GLESv2)
