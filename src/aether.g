type:       shared
modules:    A aclang allvm
link:       -lclang-cpp -lclang -lLLVM -llldb
import:     LLVM:llvm-project/bd7db754895ed3b51388ec549cd656c770c17587 native
    -S $SILVER/checkout/llvm-project/llvm
    { f'-DCMAKE_SYSROOT=$IMPORT' if SDK != 'native' else '' }
    -DCMAKE_C_COMPILER=gcc
    -DCMAKE_CXX_COMPILER=g++
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_CXX_STANDARD=17
    -DCMAKE_CXX_STANDARD_REQUIRED=ON
    -DLLVM_ENABLE_ASSERTIONS=OFF
    -DLLVM_ENABLE_PROJECTS='clang;lld;lldb'
    -DLLVM_ENABLE_RUNTIMES=''
    { '-DCLANG_CONFIG_FILE_SYSTEM_DIR=/etc/clang'   if lin else '' }
    { '-DCLANG_DEFAULT_CXX_STDLIB=libstdc++'        if lin else '' }
    { '-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON'         if not win else '' }
    -DLLVM_TOOL_GOLD_BUILD=OFF
    -DLLVM_ENABLE_FFI=OFF
    -DLLVM_ENABLE_RTTI=OFF
    { '-DLLVM_BINUTILS_INCDIR=/usr/include' if lin else '' }
    -DCLANG_DEFAULT_PIE_ON_LINUX={'ON' if lin else 'OFF'}
    -DBUILD_SHARED_LIBS=OFF
    -DLLDB_ENABLE_PYTHON=OFF
    -DLLVM_TARGETS_TO_BUILD="host;AArch64"
    -DLLVM_INCLUDE_TESTS=OFF
    -DLLVM_BUILD_LLVM_DYLIB=ON
    -DLLVM_LINK_LLVM_DYLIB=ON

import: LLVM:llvm-project/bd7db754895ed3b51388ec549cd656c770c17587 as runtimes
    -S $SILVER/checkout/llvm-project/runtimes
    -DCMAKE_C_COMPILER=$NATIVE/bin/clang
    -DCMAKE_CXX_COMPILER=$NATIVE/bin/clang++
    -DCMAKE_CXX_FLAGS="-fexceptions -funwind-tables -Wno-unused-command-line-argument"
    -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld"
    -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld"
    -DLLVM_ENABLE_RUNTIMES="compler-rt;libcxx;libcxxabi;libunwind"
    -DLIBUNWIND_ENABLE_SHARED=OFF
    -DLIBUNWIND_ENABLE_STATIC=ON
    -DCMAKE_LINKER="$NATIVE/bin/ld.lld"

import: KhronosGroup:Vulkan-Headers

import: KhronosGroup:Vulkan-Tools
    -DVULKAN_HEADERS_INSTALL_DIR=$IMPORT
