add_library (obj obj.c base.c enum.c list.c str.c vec.c prop.c prim.c pairs.c matrix.c auto.c llist.c)
set_property(TARGET obj PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories (obj PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(obj m)
