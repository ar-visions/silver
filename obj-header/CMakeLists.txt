add_executable (obj-header header.c llist.c)

if(CPP)
    set_source_files_properties (header.c PROPERTIES LANGUAGE CXX)
endif()

if(MSVC)
    add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
endif()