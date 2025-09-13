#!/bin/bash
set -e

if [ -z "$IMPORT" ]; then
    IMPORT="$(realpath "$(dirname "$0")")"
fi

LLVM_URL="https://github.com/LLVM/llvm-project"
LLVM_COMMIT="bd7db75"
build_dir="$IMPORT/build/llvm-project"
llvm_src="$IMPORT/checkout/llvm-project"
llvm_build="$build_dir/release"
mkdir -p "$IMPORT/checkout"
mkdir -p "$IMPORT/bin"

# download source for ninja and build
if ! [ -f "$IMPORT/bin/ninja" ]; then
    ninja_f="v1.13.1"
    NINJA_URL="https://github.com/ninja-build/ninja/archive/refs/tags/${ninja_f}.zip"
    cd $IMPORT/checkout
    curl -LO $NINJA_URL
    unzip -o "${ninja_f}.zip"
    cd ninja-1.13.1
    cmake -Bbuild-cmake -DBUILD_TESTING=OFF
    cmake --build build-cmake
    cp -a build-cmake/ninja $IMPORT/bin/ninja
fi

cd $IMPORT

# build LLVM from scratch
build_llvm() {
    mkdir -p "$build_dir"               # create build dir
    if [[ -d "$llvm_src" ]]; then       # clone or update
        cd "$llvm_src"
        git fetch origin
        git checkout "$LLVM_COMMIT"
        git pull origin "$LLVM_COMMIT" 2>/dev/null || true
    else
        git clone "$LLVM_URL" "$llvm_src"
        cd "$llvm_src"
        git checkout "$LLVM_COMMIT"
    fi
    
    mkdir -p "$llvm_build"              # configure
    cd "$llvm_build"
    local cmake_args=(
        -S $llvm_src/llvm
        -DCMAKE_C_COMPILER="clang"
        -DCMAKE_CXX_COMPILER="clang++"
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=$IMPORT
        -G Ninja
        -DCMAKE_CXX_STANDARD=17
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
        -DCMAKE_CXX_EXTENSIONS=OFF
        -DLLVM_ENABLE_ASSERTIONS=ON
        -DLLVM_ENABLE_PROJECTS='clang;lld;lldb;compiler-rt'
        -DLLVM_ENABLE_RUNTIMES='libcxx;libcxxabi;libunwind'
        -DLLVM_TOOL_GOLD_BUILD=OFF
        -DLLVM_ENABLE_FFI=OFF
        -DLLVM_ENABLE_RTTI=ON
        -DLLVM_BINUTILS_INCDIR=/usr/include
        -DCLANG_DEFAULT_PIE_ON_LINUX=ON
        -DCLANG_CONFIG_FILE_SYSTEM_DIR=/etc/clang
        -DBUILD_SHARED_LIBS=OFF
        -DLLDB_ENABLE_PYTHON=ON
        -DLLVM_ENABLE_PYTHON=ON
        -DPython3_EXECUTABLE=$(which python3)
        -DLLVM_TARGETS_TO_BUILD='host;AArch64'
        -DCLANG_DEFAULT_CXX_STDLIB=libstdc++
        -DLLVM_INCLUDE_TESTS=OFF
        -DCOMPILER_RT_INCLUDE_TESTS=OFF
        -DLLVM_BUILD_LLVM_DYLIB=ON
        -DLLVM_LINK_LLVM_DYLIB=ON
        -DCOMPILER_RT_BUILD_SANITIZERS=ON
        -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON
        -DLLVM_DEFAULT_CXX_STDLIB=libc++
        -DCMAKE_C_COMPILER_TARGET=arm64-apple-darwin
        -DCMAKE_OSX_SYSROOT=$(xcrun --show-sdk-path)
    )

    # linux needs -DCMAKE_C_COMPILER_TARGET=x86_64-unknown-linux-gnu

    if [[ "$OSTYPE" == "darwin"* ]]; then
        SYSROOT="$(xcrun --sdk macosx --show-sdk-path 2>/dev/null || echo "")"
        cmake_args+=(-DCMAKE_OSX_SYSROOT="$SYSROOT")
    fi

    cmake "${cmake_args[@]}"
    
    # build with limited jobs, the linking stage requires lots of memory
    local num_jobs=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    if [[ $num_jobs -gt 8 ]]; then
        num_jobs=8
    fi
    
    ninja -j$num_jobs
    ninja install
}

if [[ ! -f "$IMPORT/bin/clang" ]] || [[ ! -d "$llvm_build" ]]; then
    build_llvm
fi

(cd "$(dirname "$0")" && python3 gen.py "$@")
