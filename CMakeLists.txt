cmake_minimum_required    (VERSION 2.8.12)
enable_language           (ASM)
project                   (silver)

if(NOT CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX  ${CMAKE_BINARY_DIR}/silver-import)
endif()

# effective clang usage:
# clang++ -std=c++17 -I `llvm-config-14 --cxxflags --ldflags --libs core executionengine mcjit native`


add_custom_target         (script COMMAND ${CMAKE_COMMAND} -E env bash ${CMAKE_SOURCE_DIR}/deps.sh ${CMAKE_INSTALL_PREFIX} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
find_package              (LLVM 14 REQUIRED CONFIG)
set                       (LLVM_VERSION "14")
find_package              (Clang REQUIRED CONFIG HINTS /usr/lib/cmake/clang-14)
set                       (Clang_VERSION "14")
include_directories       (${CMAKE_INSTALL_PREFIX}/include ${LLVM_INCLUDE_DIRS} /usr/include/llvm-14 ${Clang_INCLUDE_DIRS}) # /usr/lib/gcc/x86_64-linux-gnu/11/include
link_directories          (${LLVM_LIBRARY_DIRS} /usr/lib/llvm-14/lib ${Clang_LIBRARY_DIRS})
link_directories          (${CMAKE_INSTALL_PREFIX}/lib)
file                      (GLOB src "${CMAKE_CURRENT_SOURCE_DIR}/silver/*.c")
add_compile_options       (-fno-exceptions -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS)


add_compile_options       (
    -Wno-write-strings
    -Wno-compare-distinct-pointer-types
    -Wno-deprecated-declarations
    -Wno-incompatible-pointer-types
    -Wfatal-errors
    -fPIC)
#add_compile_options      (-fsanitize=address -fno-omit-frame-pointer -g)
#add_link_options         (-fsanitize=address)
add_compile_options       (-I${CMAKE_INSTALL_PREFIX}/include)
set                       (CMAKE_C_STANDARD             11)
set                       (CMAKE_CXX_STANDARD           17)
set                       (CMAKE_CXX_STANDARD_REQUIRED  ON)
set                       (CMAKE_C_COMPILER             gcc)
set                       (CMAKE_CXX_COMPILER           g++)
add_executable            (silver                       ${src})
add_dependencies          (silver                       script)
target_link_libraries     (silver                       A LLVM-14 clang-14)
target_include_directories(silver                       PRIVATE . ./silver)
