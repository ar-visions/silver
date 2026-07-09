# extern-C shim over libtorch TorchScript inference; links the
# pip-installed torch so trainer and runtime share one build
cmake_minimum_required(VERSION 3.16)
project(torch_shim CXX)
set(CMAKE_CXX_STANDARD 17)
execute_process(
    COMMAND python3 -c "import torch, os; print(os.path.dirname(torch.__file__))"
    OUTPUT_VARIABLE TORCH_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT TORCH_DIR)
    message(FATAL_ERROR "pip torch not found (python3 -c 'import torch')")
endif()
add_library(torch_shim SHARED torch_shim.cpp)
target_include_directories(torch_shim PRIVATE
    ${TORCH_DIR}/include ${TORCH_DIR}/include/torch/csrc/api/include)
target_link_directories(torch_shim PRIVATE ${TORCH_DIR}/lib)
target_link_libraries(torch_shim torch torch_cpu c10)
set_target_properties(torch_shim PROPERTIES
    BUILD_RPATH ${TORCH_DIR}/lib INSTALL_RPATH ${TORCH_DIR}/lib)
install(TARGETS torch_shim LIBRARY DESTINATION lib)
install(FILES torch_shim.h DESTINATION include)
