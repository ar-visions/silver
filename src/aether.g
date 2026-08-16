type:       shared
modules:    Au aclang allvm lldb coverage vec
link:       -lclang-cpp -lclang -lLLVM -llldb -lstdc++ -lz -ltinfo
import:     LLVM:llvm-project/bd7db754895ed3b51388ec549cd656c770c17587 native
    -S $SILVER/checkout/llvm-project/llvm
    { f'-DCMAKE_SYSROOT=$IMPORT' if SDK != 'native' else '' }
    -DCMAKE_C_COMPILER={'$NATIVE/bin/clang-cl.exe' if win else 'gcc'}
    -DCMAKE_CXX_COMPILER={'$NATIVE/bin/clang-cl.exe' if win else 'g++'}
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_CXX_STANDARD=17
    -DCMAKE_CXX_STANDARD_REQUIRED=ON
    -DLLVM_ENABLE_ASSERTIONS=OFF
    -DLLVM_ENABLE_RUNTIMES={'"compiler-rt"' if win else '"compiler-rt;libcxx;libcxxabi;libunwind"'}
    -DLLVM_ENABLE_PROJECTS='clang;lld;lldb'
    { '-DCLANG_CONFIG_FILE_SYSTEM_DIR=/etc/clang'   if lin else '' }
    { '-DCLANG_DEFAULT_CXX_STDLIB=libstdc++'        if lin else '' }
    { '-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON'         if not win else '' }
    -DLLVM_TOOL_GOLD_BUILD=OFF
    -DLLVM_ENABLE_FFI=OFF
    -DLLVM_ENABLE_RTTI=ON
    { '-DLLVM_BINUTILS_INCDIR=/usr/include' if lin else '' }
    -DCLANG_DEFAULT_PIE_ON_LINUX={'ON' if lin else 'OFF'}
    -DBUILD_SHARED_LIBS=OFF
    -DLLDB_ENABLE_PYTHON=OFF
    { '-DLLVM_TARGETS_TO_BUILD=host' if os.environ.get('SILVER_DOCKER') else '"-DLLVM_TARGETS_TO_BUILD=host;AArch64;ARM;Mips;RISCV;WebAssembly"' }
    -DLLVM_INCLUDE_TESTS=OFF
    -DLLVM_BUILD_LLVM_DYLIB=ON
    -DLLVM_LINK_LLVM_DYLIB=ON

import: LLVM:llvm-project/bd7db754895ed3b51388ec549cd656c770c17587 as runtimes
    -S $SILVER/checkout/llvm-project/runtimes
    -DCMAKE_C_COMPILER=$NATIVE/bin/{'clang-cl.exe' if win else 'clang'}
    -DCMAKE_CXX_COMPILER=$NATIVE/bin/{'clang-cl.exe' if win else 'clang++'}
    -DCMAKE_CXX_FLAGS={'"/EHsc"' if win else '"-fexceptions -funwind-tables -Wno-unused-command-line-argument"'}
    -DCMAKE_EXE_LINKER_FLAGS={'"/DEFAULTLIB:msvcprt"' if win else '"-fuse-ld=lld"'}
    -DCMAKE_SHARED_LINKER_FLAGS={'"/DEFAULTLIB:msvcprt"' if win else '"-fuse-ld=lld"'}
    -DLLVM_ENABLE_RUNTIMES={'"compiler-rt;libcxx"' if win else '"compiler-rt;libcxx;libcxxabi;libunwind"'}
    -DCMAKE_LINKER="$NATIVE/bin/{'lld-link.exe' if win else 'ld.lld'}"
    { '-DCMAKE_BUILD_TYPE=Release' if win else '' }
    { '-DCMAKE_CXX_STANDARD=17'    if win else '' }
    { '-DLIBCXX_CXX_ABI=vcruntime' if win else '' }
    { '-DCOMPILER_RT_EXCLUDE_ATOMIC_BUILTIN=OFF' if win else '' }
    { '' if win else '-DLIBCXX_HAS_NO_STDLIB=ON' }
    { '' if win else '-DCLANG_DEFAULT_CXX_STDLIB=libstdc++' }
    { '' if win else '-DCMAKE_CXX_STANDARD_LIBRARY=libstdc++' }
    { '' if win else '-DLIBUNWIND_ENABLE_SHARED=OFF' }
    { '' if win else '-DLIBUNWIND_ENABLE_STATIC=ON' }
