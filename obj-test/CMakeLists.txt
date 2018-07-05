file(GLOB src "*.c")
add_executable (obj-test ${src})

if(CPP)
    set_source_files_properties (${src} PROPERTIES LANGUAGE CXX)
endif()

if(MSVC)
    add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
endif()

target_link_libraries (obj-test LINK_PUBLIC obj)
target_include_directories (obj-test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)