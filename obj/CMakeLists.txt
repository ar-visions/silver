file(GLOB src "*.c")
add_library (obj SHARED ${src})

if(CPP)
    set_source_files_properties (${src} PROPERTIES LANGUAGE CXX)
endif()

if(MSVC)
    add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
endif()

set_property(TARGET obj PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories (obj PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

if(UNIX)
    target_link_libraries(obj m)
endif()

file(GLOB src "*.c")

file(GLOB include_files "*.h")

foreach ( file ${include_files} )
    get_filename_component( name ${file} NAME )
    install( FILES ${file} DESTINATION include/obj )
endforeach()

install (TARGETS obj LIBRARY DESTINATION lib)
