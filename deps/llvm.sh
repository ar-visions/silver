#!/bin/bash
# deps.sh - Lazy LLVM downloader/builder that always returns linker flags
# Builds LLVM if needed, then outputs the -l library list

set -e

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="$PROJECT_ROOT/install"
LLVM_URL="https://github.com/llvm/llvm-project"
LLVM_COMMIT="main"

# LLVM libraries we need (from your silver import config)
LLVM_LIBS=(
    "m"
    "clang" 
    "LLVMCore"
    "LLVMBitReader"
    "LLVMBitWriter"
    "LLVMIRReader"
    "LLVMSupport"
    "LLVMExecutionEngine"
    "LLVMTarget"
    "LLVMTargetParser"
    "LLVMTransformUtils"
    "LLVMAnalysis"
    "LLVMProfileData"
    "LLVMAArch64AsmParser"
    "LLVMAArch64CodeGen"
    "LLVMAArch64Desc"
    "LLVMAArch64Info"
    "LLVMAArch64Utils"
    "LLVMX86CodeGen"
    "LLVMX86AsmParser"
    "LLVMX86Desc"
    "LLVMX86Info"
)

# Detect platform
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="darwin"
    SYSROOT="$(xcrun --sdk macosx --show-sdk-path 2>/dev/null || echo "")"
else
    PLATFORM="linux"
fi

# Check if LLVM is already built
is_llvm_built() {
    [[ -d "$INSTALL_DIR/lib" ]] && [[ -f "$INSTALL_DIR/bin/clang" ]]
}

# Build LLVM from scratch
build_llvm() {
    echo "Building LLVM from scratch..." >&2
    
    local build_dir="$PROJECT_ROOT/build"
    local llvm_src="$build_dir/llvm-project"
    local llvm_build="$build_dir/llvm-build"
    
    # Create directories
    mkdir -p "$build_dir" "$INSTALL_DIR"
    
    # Clone or update LLVM
    if [[ -d "$llvm_src" ]]; then
        echo "Updating LLVM source..." >&2
        cd "$llvm_src"
        git fetch origin
        git checkout "$LLVM_COMMIT"
        git pull origin "$LLVM_COMMIT" 2>/dev/null || true
    else
        echo "Cloning LLVM..." >&2
        git clone "$LLVM_URL" "$llvm_src"
        cd "$llvm_src"
        git checkout "$LLVM_COMMIT"
    fi
    
    # Configure build
    mkdir -p "$llvm_build"
    cd "$llvm_build"
    
    echo "Configuring LLVM..." >&2
    
    # Base CMake arguments (from your silver import config)
    local cmake_args=(
        -S "$llvm_src/llvm"
        -G Ninja
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"
        -DLLVM_ENABLE_ASSERTIONS=OFF
        -DLLVM_ENABLE_PROJECTS="clang;lld;lldb;compiler-rt"
        -DLLVM_TOOL_GOLD_BUILD=ON
        -DLLVM_ENABLE_FFI=OFF
        -DLLVM_ENABLE_THREADS=ON
        -DLLVM_PARALLEL_LINK_JOBS=1
        -DLLVM_BUILD_TOOLS=ON
        -DLLVM_ENABLE_LTO=OFF
        -DLLDB_INCLUDE_TESTS=OFF
        -DLLDB_EXPORT_ALL_SYMBOLS=ON
        -DLLVM_ENABLE_RTTI=OFF
        -DCLANG_DEFAULT_PIE_ON_LINUX=ON
        -DCLANG_CONFIG_FILE_SYSTEM_DIR=/etc/clang
        -DLLVM_ENABLE_LIBCXX=OFF
        -DBUILD_SHARED_LIBS=ON
        -DLLDB_ENABLE_PYTHON=OFF
        -DLLVM_TARGETS_TO_BUILD="host;X86;AArch64"
    )
    
    # Platform-specific arguments
    if [[ "$PLATFORM" == "darwin" ]]; then
        cmake_args+=(
            -DCMAKE_CXX_FLAGS="-stdlib=libc++"
            -DCLANG_DEFAULT_CXX_STDLIB=libc++
        )
        if [[ -n "$SYSROOT" ]]; then
            cmake_args+=(-DDEFAULT_SYSROOT="$SYSROOT")
        fi
    else
        cmake_args+=(
            -DCLANG_DEFAULT_CXX_STDLIB=libstdc++
            -DLLVM_BINUTILS_INCDIR=/usr/include
        )
    fi
    
    # Configure
    cmake "${cmake_args[@]}"
    
    # Build (limit jobs to avoid OOM)
    echo "Building LLVM (this will take a while)..." >&2
    local num_jobs=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    if [[ $num_jobs -gt 4 ]]; then
        num_jobs=4
    fi
    
    ninja -j$num_jobs
    
    # Install
    echo "Installing LLVM..." >&2
    ninja install
    
    echo "LLVM build complete!" >&2
}

# Generate linker flags
get_llvm_flags() {
    local flags=""
    
    # Add library path and rpath
    if [[ "$PLATFORM" == "darwin" ]]; then
        flags="-L$INSTALL_DIR/lib -Wl,-rpath,@executable_path/../lib -Wl,-rpath,$INSTALL_DIR/lib"
    else
        flags="-L$INSTALL_DIR/lib -Wl,-rpath,\$ORIGIN/../lib -Wl,-rpath,$INSTALL_DIR/lib"
    fi
    
    # Add all library flags
    for lib in "${LLVM_LIBS[@]}"; do
        flags="$flags -l$lib"
    done
    
    echo "$flags"
}

# Main function - lazy build and return flags
main() {
    # If LLVM not built, build it
    if ! is_llvm_built; then
        build_llvm
    fi
    
    # Always return the flags
    get_llvm_flags
}

main "$@"