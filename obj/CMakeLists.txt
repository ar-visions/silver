file(GLOB src "build/obj/*.c")
add_library (obj SHARED ${src})

if(CPP)
    set_source_files_properties (${src} PROPERTIES LANGUAGE CXX)
endif()

if(MSVC)
    add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
endif()

set_property(TARGET obj PROPERTY POSITION_INDEPENDENT_CODE ON)

if(UNIX)
    target_link_libraries(obj m)
endif()

