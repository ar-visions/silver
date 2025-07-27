cmake_minimum_required(VERSION 3.22)

# bootstrap must be invoked first; verify we have LLVM
set(CLANG_EXE        ${CMAKE_CURRENT_LIST_DIR}/bin/clang)
set(CLANGCPP_EXE     ${CMAKE_CURRENT_LIST_DIR}/bin/clang++)
if(WIN32)
    set(CLANG_EXE               ${CLANG_EXE}.exe)
    set(CLANGCPP_EXE            ${CLANGCPP_EXE}.exe)
    set(CMAKE_MAKE_PROGRAM      ${CMAKE_CURRENT_LIST_DIR}/build/ninja/ninja)
endif()

set(CMAKE_C_COMPILER            ${CLANG_EXE})
set(CMAKE_CXX_COMPILER          ${CLANGCPP_EXE})
set(CMAKE_C_STANDARD            11)
set(CMAKE_C_STANDARD_REQUIRED   ON)
set(CMAKE_C_EXTENSIONS          ON)
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

project(silver C)

# configurable input
set(IMPORT "${CMAKE_CURRENT_LIST_DIR}")

# Include paths
include_directories(
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_BINARY_DIR}/app
  ${IMPORT}/include
)

# glob sources
file(GLOB SRC_C  src/*.c)
file(GLOB SRC_CC src/*.cc)
file(GLOB APP_C  app/*.c)
file(GLOB APP_CC app/*.cc)

# simplifies build caching
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${IMPORT}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${IMPORT}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${IMPORT}/bin)

set(BASE_C_FLAGS -fPIC -Wno-write-strings -Wno-compare-distinct-pointer-types
    -Wno-deprecated-declarations -Wno-incompatible-pointer-types -Wno-shift-op-parentheses
    -Wfatal-errors -fvisibility=default)

set(BASE_CXX_FLAGS          "${BASE_C_FLAGS} -std=c++17")
set(CMAKE_C_FLAGS_DEBUG     "${BASE_C_FLAGS} -g2 -O0")
set(CMAKE_C_FLAGS_RELEASE   "${BASE_C_FLAGS} -O2")
set(CMAKE_CXX_FLAGS_DEBUG   "${BASE_CXX_FLAGS} -g2 -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${BASE_CXX_FLAGS} -O2")
set(CMAKE_C_COMPILER_ID_RUN     TRUE)
set(CMAKE_CXX_COMPILER_ID_RUN   TRUE)
set(CMAKE_GENERATOR_TOOLSET     "")

# run headers.py before build
set(HEADER_ENV_VARS
    "PROJECT_PATH=${IMPORT}"
    "BUILD_PATH=${CMAKE_BINARY_DIR}"
    "PROJECT=${PROJECT_NAME}"
    "IMPORT=${IMPORT}"
)

find_program(PYTHON_EXECUTABLE
    NAMES python
    PATHS ${IMPORT}/build/python
    NO_DEFAULT_PATH
)

if(NOT PYTHON_EXECUTABLE)
    find_program(PYTHON_EXECUTABLE python3 python)
endif()

if(NOT PYTHON_EXECUTABLE)
    message(FATAL_ERROR "python not found (install with package manager)")
endif()

add_custom_target(src_headers
    COMMAND ${CMAKE_COMMAND} -E env 
        ${HEADER_ENV_VARS} "DIRECTIVE=src"
        ${PYTHON_EXECUTABLE} ${IMPORT}/support/headers.py
    WORKING_DIRECTORY ${IMPORT}
)

add_custom_target(app_headers
    COMMAND ${CMAKE_COMMAND} -E env 
        ${HEADER_ENV_VARS} "DIRECTIVE=app"
        ${PYTHON_EXECUTABLE} ${IMPORT}/support/headers.py
    WORKING_DIRECTORY ${IMPORT}
)

# Shared library from src
add_library                 (${PROJECT_NAME} SHARED ${SRC_C} ${SRC_CC})
add_dependencies            (${PROJECT_NAME} src_headers)
target_compile_definitions  (${PROJECT_NAME} PRIVATE MODULE="\"${PROJECT_NAME}\"")

# Executables from app/*
foreach                     (app ${APP_C} ${APP_CC})
  get_filename_component    (appname ${app} NAME_WE)
  add_executable            (${appname}_app ${app})
  set_target_properties     (${appname}_app PROPERTIES OUTPUT_NAME "${appname}")
  add_dependencies          (${appname}_app app_headers)
  target_link_libraries     (${appname}_app ${PROJECT_NAME})
  target_compile_definitions(${appname}_app PRIVATE MODULE="\"${PROJECT_NAME}\"")
endforeach()

# Platform specific linker settings
if(APPLE)
  target_link_options       (${PROJECT_NAME} PRIVATE -Wl,-install_name,@rpath/lib${PROJECT_NAME}.dylib)
  foreach                   (app ${APP_C} ${APP_CC})
    get_filename_component  (appname ${app} NAME_WE)
    target_link_options     (${appname}_app PRIVATE -Wl,-rpath,@executable_path/../lib)
  endforeach()
else()
  target_link_options       (${PROJECT_NAME} PRIVATE -Wl,-soname,lib${PROJECT_NAME}.so)
  foreach                   (app ${APP_C} ${APP_CC})
    get_filename_component  (appname ${app} NAME_WE)
    target_link_options     (${appname}_app PRIVATE -Wl,-rpath,\$ORIGIN/../lib)
  endforeach()
endif()

link_directories(${IMPORT}/lib)
