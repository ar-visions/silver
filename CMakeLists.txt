cmake_minimum_required(VERSION 2.8.12)
project(silver)

# select all of mx into silver; we'll add functionality as C99 interface functions that the scripts will use
# theres nothing complex in the modules, so the source can simply be selected

file(GLOB_RECURSE mx_src "${CMAKE_CURRENT_SOURCE_DIR}/deps/mx/**/*.cpp")

add_compile_options       (-Wno-write-strings)

#add_compile_options       (-fsanitize=address -fno-omit-frame-pointer -g)
#add_link_options          (-fsanitize=address)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_library               (silver_library   SHARED ${mx_src})
add_executable            (silver_generator silver/silver.cpp)
target_link_libraries     (silver_generator silver_library)
set_target_properties     (silver_library   PROPERTIES OUTPUT_NAME silver)
set_target_properties     (silver_generator PROPERTIES OUTPUT_NAME silver)
target_include_directories(silver_library   PRIVATE . ./deps/mx)
target_include_directories(silver_generator PRIVATE . ./deps/mx)