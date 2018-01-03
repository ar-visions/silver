
file(GLOB src "build/obj-math/*.c")
add_library (obj-math SHARED ${src})

if(CPP)
    set_source_files_properties (${src} PROPERTIES LANGUAGE CXX)
endif()

if(MSVC)
    add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
endif()

set_property(TARGET obj-math PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries (obj-math LINK_PUBLIC obj m)
