file(GLOB src "*.c")
add_executable (obj-cx ${src})

if(CPP)
    set_source_files_properties (${src} PROPERTIES LANGUAGE CXX)
endif()

if(MSVC)
    add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
endif()

target_link_libraries (obj-cx LINK_PUBLIC obj-math)