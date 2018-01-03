add_library (obj-poly2tri SHARED primitives.c advancing_front.c sweep_context.c sweep.c poly2tri.c)

if(CPP)
    set_source_files_properties (${src} PROPERTIES LANGUAGE CXX)
endif()

if(MSVC)
    add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
endif()

set_property(TARGET obj-poly2tri PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries (obj-poly2tri LINK_PUBLIC obj-math m)
