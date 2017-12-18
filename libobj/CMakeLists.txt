add_library (libobj obj.c base.c enum.c list.c str.c vec.c prop.c prim.c pairs.c matrix.c auto.c llist.c)
set_property(TARGET libobj PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories (libobj PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(libobj m)
