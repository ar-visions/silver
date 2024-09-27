cmake_minimum_required    (VERSION 2.8.12)
enable_language           (ASM)
project                   (silver)

if(NOT CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX  ${CMAKE_BINARY_DIR}/silver-import)
endif()

set                       (LLVM_SM "18")
set                       (LLVM_LA "18.1.8")
# built LLVM 19 by debug
#set                      (LLVM_SM "19")
#set                      (LLVM_LA "19.1.0")
#
add_custom_target         (script COMMAND ${CMAKE_COMMAND} -E env bash ${CMAKE_SOURCE_DIR}/deps.sh ${CMAKE_INSTALL_PREFIX} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

set                       (LLVM_DIR  "/usr/local/llvm-${LLVM_SM}/lib/cmake/llvm")
set                       (Clang_DIR "/usr/local/llvm-${LLVM_SM}/lib/cmake/clang")
find_package              (LLVM  ${LLVM_LA} REQUIRED CONFIG)
find_package              (Clang ${LLVM_LA} REQUIRED CONFIG)

include_directories       (${LLVM_INCLUDE_DIRS} ${Clang_INCLUDE_DIRS} ${CMAKE_INSTALL_PREFIX}/include)
link_directories          (${LLVM_LIBRARY_DIRS} ${Clang_LIBRARY_DIRS} ${CMAKE_INSTALL_PREFIX}/lib)

message(STATUS "is this printing? dirs = ${LLVM_INCLUDE_DIRS} ${Clang_INCLUDE_DIRS} ... ${LLVM_LIBRARY_DIRS} ${Clang_LIBRARY_DIRS}")

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
target_link_libraries     (silver                       A /usr/local/llvm-${LLVM_SM}/lib/libLLVM.so /usr/local/llvm-${LLVM_SM}/lib/libclang.so)
target_include_directories(silver                       PRIVATE . ./silver)
