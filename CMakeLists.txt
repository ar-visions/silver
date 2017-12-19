cmake_minimum_required (VERSION 2.8.11)
project (LIBOBJ)
add_subdirectory (obj)
add_subdirectory (obj-test)
set_property(TARGET obj PROPERTY CXX_STANDARD 11)
#set_property(TARGET obj PROPERTY C_STANDARD 99)

