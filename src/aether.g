type:       shared
modules:    A aclang allvm
link:       -lclang-cpp -lclang -lLLVM -llldb
import:     llvm-project https://github.com/LLVM/llvm-project bd7db754895ed3b51388ec549cd656c770c17587
    -S $SILVER/checkout/llvm-project/llvm
    -DCMAKE_C_COMPILER=/usr/bin/gcc
    -DCMAKE_CXX_COMPILER=/usr/bin/g++
    -DCMAKE_ASM_COMPILER=/usr/bin/gcc
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX=$IMPORT
    -DCMAKE_CXX_STANDARD=17
    -DCMAKE_CXX_STANDARD_REQUIRED=ON
    -DLLVM_ENABLE_ASSERTIONS=OFF
    -DLLVM_ENABLE_PROJECTS='clang;lld;lldb'
    -DLLVM_RUNTIME_TARGETS="AArch64;X86"
    -DLLVM_ENABLE_RUNTIMES={ 'compiler-rt' if win else "'libcxx;libcxxabi;libunwind;compiler-rt'" }
    { '-DCLANG_CONFIG_FILE_SYSTEM_DIR=/etc/clang'   if lin else '' }
    { '-DCLANG_DEFAULT_CXX_STDLIB=libstdc++'        if lin else '' }
    { '-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON'         if not win else '' }
    -DLLVM_TOOL_GOLD_BUILD=OFF
    -DLLVM_ENABLE_FFI=OFF
    -DLLVM_ENABLE_RTTI=ON
    -DLLVM_ENABLE_EH=ON
    -DRUNTIMES_AArch64_CMAKE_ARGS="-DLIBCXXABI_USE_LLVM_UNWINDER=ON;-DLLVM_ENABLE_EH=ON;-DLLVM_ENABLE_RTTI=ON;-DCMAKE_CXX_FLAGS=-fexceptions;-DCMAKE_C_FLAGS=-funwind-tables"
    -DRUNTIMES_X86_CMAKE_ARGS="-DLIBCXXABI_USE_LLVM_UNWINDER=ON;-DLLVM_ENABLE_EH=ON;-DLLVM_ENABLE_RTTI=ON;-DCMAKE_CXX_FLAGS=-fexceptions;-DCMAKE_C_FLAGS=-funwind-tables"
    -DCMAKE_CXX_FLAGS="-fexceptions -funwind-tables -frtti"
    -DCMAKE_C_FLAGS="-funwind-tables"
    { '-DLLVM_BINUTILS_INCDIR=/usr/include' if lin else '' }
    -DCLANG_DEFAULT_PIE_ON_LINUX={'ON' if lin else 'OFF'}
    -DBUILD_SHARED_LIBS=OFF
    -DLLDB_ENABLE_PYTHON=OFF
    -DLLVM_TARGETS_TO_BUILD="AArch64;X86;RISCV;Mips"
    -DLLVM_INCLUDE_TESTS=OFF
    -DCOMPILER_RT_INCLUDE_TESTS=OFF
    -DLLVM_BUILD_LLVM_DYLIB=OFF
    -DLLVM_LINK_LLVM_DYLIB=OFF
    -DCOMPILER_RT_BUILD_SANITIZERS=OFF
    -DCOMPILER_RT_DEFAULT_TARGET_ONLY=OFF
    -DCOMPILER_RT_BUILD_BUILTINS=ON
