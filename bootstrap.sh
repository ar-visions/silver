#!/usr/bin/env bash
set -e

if [ -z "$IMPORT" ]; then
    IMPORT="$(realpath "$(dirname "$0")")"
fi

# lets make sure silver bins come first
export PATH=$IMPORT/bin:$PATH

LLVM_URL="https://github.com/LLVM/llvm-project"
LLVM_COMMIT="bd7db75"
llvm_build_dir="$IMPORT/build/llvm-project"
llvm_src="$IMPORT/checkout/llvm-project"
llvm_build="$llvm_build_dir/release"

mbed_url="https://github.com/Mbed-TLS/mbedtls"
mbed_build_dir="$IMPORT/build/mbedtls"
mbed_src="$IMPORT/checkout/mbedtls"
mbed_build="$mbed_build_dir/release"
mbed_commit="ec40440"

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

if ! [ -f "$IMPORT/bin/python3" ]; then
    PY_VER="3.11.9"
    PY_SRC="$IMPORT/checkout/Python-$PY_VER"

    mkdir -p "$IMPORT/checkout"
    cd "$IMPORT/checkout"
    curl -LO "https://www.python.org/ftp/python/$PY_VER/Python-$PY_VER.tgz"
    tar -xf "Python-$PY_VER.tgz"
    cd "Python-$PY_VER"

    CC=gcc ./configure --prefix=$IMPORT --with-ensurepip=install
    make -j$(sysctl -n hw.ncpu)
    make install
fi

cd $IMPORT

 if [ -n "$ZSH_VERSION" ]; then
    rehash 2>/dev/null || true
elif [ -n "$BASH_VERSION" ]; then
    hash -r 2>/dev/null || true
elif [ -n "$FISH_VERSION" ]; then
    builtin functions -q rehash && rehash 2>/dev/null || true
elif [ -n "$TCSH_VERSION" ] || [ -n "$CSH_VERSION" ]; then
    rehash 2>/dev/null || true
else
    # fallback: force PATH refresh by launching a subshell
    exec $SHELL -l
fi

# we need https as a core dependency, since silver uses chatgpt, claude, and gemini apis
# wolf-ssl is a bit easier to use, however mbed works with streaming protocol
# while streamining is not in the domain of silver, its generally in the stack of apps
# all of these core dependencies are there for silver apps to use

build_mbed() {
    mkdir -p "$mbed_build_dir"
    
    if [[ -d "$mbed_src" ]]; then       # clone or update
        cd "$mbed_src"
        git fetch origin
        git checkout "$mbed_commit"
        git pull origin "$mbed_commit" 2>/dev/null || true
    else
        git clone "$mbed_url" "$mbed_src"
        cd "$mbed_src"
        git checkout "$mbed_commit"
        git submodule update --init --recursive
        pip3 install jsonschema jinja2
    fi
    
    mkdir -p "$mbed_build"              # configure
    cd "$mbed_build"
    local cmake_args=(
        -S $mbed_src
        -DPython3_EXECUTABLE=$IMPORT/bin/python3
        -DCMAKE_C_COMPILER="clang"
        -DCMAKE_CXX_COMPILER="clang++"
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=$IMPORT
        -G Ninja
        -DENABLE_TESTING=0
        -DPSA_CRYPTO_DRIVERS=0
        -DCMAKE_POSITION_INDEPENDENT_CODE=1
        -DLINK_WITH_PTHREAD=1
    )

    # linux needs -DCMAKE_C_COMPILER_TARGET=x86_64-unknown-linux-gnu
    if [[ "$OSTYPE" == "darwin"* ]]; then
        SYSROOT="$(xcrun --sdk macosx --show-sdk-path 2>/dev/null || echo "")"
        cmake_args+=(-DCMAKE_OSX_SYSROOT="$SYSROOT")
    fi

    cmake "${cmake_args[@]}"
    local num_jobs=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    ninja -j$num_jobs
    ninja install
}

# build LLVM from scratch; we cannot depend on various versions used throughout os's
# they do not all contain the runtimes, clang++ APIs, and IR API required
# it would be ludacrous to call different versions on different systems for the same given silver version
# that said, different system headers are used for C++ and that must be the case since C++ is not implemented
# in LLVM clang; they use the SDK on the system
# for now, C++ integration has hurdles and is planned for 1.0

build_llvm() {
    mkdir -p "$llvm_build_dir"               # create build dir
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
    )
    # -DCMAKE_OSX_SYSROOT=$(xcrun --show-sdk-path)

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

if [[ ! -d "$mbed_build" ]]; then
    build_mbed
fi

(cd "$(dirname "$0")" && cd $IMPORT && python3 gen.py "$@")
