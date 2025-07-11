#!/bin/bash
# deps.sh - Lazy LLVM downloader/builder that always returns linker flags
# Builds LLVM if needed, then outputs the -l library list

set -e

# attempting to use 20.1 on arch
echo "-lLLVM"
exit 0

# Configuration (if we want to use )

LLVM_URL="https://github.com/ar-visions/llvm-project"
LLVM_COMMIT="d37feb7"

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
    # First check if LLVM exists at all
    [[ -d "$IMPORT/lib" ]] && [[ -f "$IMPORT/bin/clang" ]] && [[ -d "$IMPORT/checkout/llvm-build" ]] || return 1
    
    # Get the newest file time in LLVM build
    local llvm_newest=$(find "$IMPORT/checkout/llvm-build" -type f -printf '%T@\n' 2>/dev/null | sort -n | tail -1)
    
    # Get the newest file time in source directory
    local src_newest=$(find "$SRC_DIRECTIVE" -type f -printf '%T@\n' 2>/dev/null | sort -n | tail -1)
    
    # If we couldn't get times, assume rebuild needed
    [[ -n "$llvm_newest" ]] && [[ -n "$src_newest" ]] || return 1
    
    # LLVM is built if its newest file is newer than source newest file
    (( $(echo "$llvm_newest > $src_newest" | bc -l) ))
}

# Build LLVM from scratch
build_llvm() {
    local build_dir="$IMPORT/checkout"
    local llvm_src="$build_dir/llvm-project"
    local llvm_build="$build_dir/llvm-build"
    
    # Create directories
    mkdir -p "$build_dir" "$IMPORT"
    
    # Clone or update LLVM
    if [[ -d "$llvm_src" ]]; then
        cd "$llvm_src"
        git fetch origin
        git checkout "$LLVM_COMMIT"
        git pull origin "$LLVM_COMMIT" 2>/dev/null || true
    else
        git clone "$LLVM_URL" "$llvm_src"
        cd "$llvm_src"
        git checkout "$LLVM_COMMIT"
    fi
    
    # Configure build
    mkdir -p "$llvm_build"
    cd "$llvm_build"
    
    # Base CMake arguments (from your silver import config)
    local cmake_args=(
        -S "$llvm_src/llvm"
        -DCMAKE_C_COMPILER="gcc" 
        -DCMAKE_CXX_COMPILER="g++" 
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX="$IMPORT"
        -G Ninja
        -DLLVM_ENABLE_ASSERTIONS=ON
        -DLLVM_ENABLE_PROJECTS='clang;lld;lldb'
        -DLLVM_TOOL_GOLD_BUILD=OFF
        -DLLVM_ENABLE_FFI=OFF
        -DLLVM_ENABLE_RTTI=ON
        -DLLVM_BINUTILS_INCDIR=/usr/include
        -DCLANG_DEFAULT_PIE_ON_LINUX=ON
        -DCLANG_CONFIG_FILE_SYSTEM_DIR=/etc/clang
        -DLLVM_ENABLE_LIBCXX=OFF
        -DBUILD_SHARED_LIBS=ON
        -DLLDB_ENABLE_PYTHON=OFF
        -DLLVM_BUILD_LLVM_DYLIB=OFF
        -DLLVM_LINK_LLVM_DYLIB=OFF
        -DLLVM_TARGETS_TO_BUILD='host;X86;AArch64'
        -DCLANG_DEFAULT_CXX_STDLIB=libstdc++
        
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
    
    echo "something"
    # Force consistent compilers
    cd $llvm_build
    cmake "${cmake_args[@]}"
    
    # Build (limit jobs to avoid OOM)
    local num_jobs=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    if [[ $num_jobs -gt 4 ]]; then
        num_jobs=4
    fi
    
    ninja -j$num_jobs
    ninja install
}

# Generate linker flags
get_llvm_flags() {
    local flags=""
    
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