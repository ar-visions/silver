cmake_minimum_required (VERSION 3.0)
project (LIBOBJ)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wfatal-errors")
set(include_dirs "")
set(mods "")

macro(mod module_name)
    add_subdirectory(${module_name})
    list(APPEND include_dirs "${CMAKE_CURRENT_SOURCE_DIR}/${module_name}/build")
    list(APPEND mods ${module_name})
endmacro()

# get header parsing working

add_subdirectory(obj-header)

mod(obj)
mod(obj-math)
#mod(obj-poly2tri)
#mod(obj-gfx)
#mod(obj-ui)
mod(obj-test)

foreach(module ${mods})
    target_include_directories(${module} PUBLIC ${include_dirs})
endforeach()

if(MSVC)
    if(NOT CPP)
        set(CPP 11)
    endif()
endif()

if(CPP)
    set_property(TARGET obj PROPERTY CXX_STANDARD ${CPP})
    set_property(TARGET obj-test PROPERTY CXX_STANDARD ${CPP})
else()
    set_property(TARGET obj PROPERTY C_STANDARD 99)
    set_property(TARGET obj-test PROPERTY C_STANDARD 99)
endif()
