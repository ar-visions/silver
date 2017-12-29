add_executable (obj-header header.c)

if(CPP)
    set_source_files_properties (header.c PROPERTIES LANGUAGE CXX)
endif()
