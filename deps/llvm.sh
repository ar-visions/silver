#!/bin/bash
# deps.sh - Lazy LLVM downloader/builder that always returns linker flags
# Builds LLVM if needed, then outputs the -l library list

set -e

# attempting to use 20.1 on arch
#echo "-lLLVM"
#exit 0

# Configuration (if we want to use )

LLVM_URL="https://github.com/ar-visions/llvm-project"
LLVM_COMMIT="d37feb7"

# LLVM libraries we need (from your silver import config)
LLVM_LIBS=(
    "m"
    "clang"
    "lldb"
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

build_dir="$IMPORT/checkout"
llvm_src="$build_dir/llvm-project"
llvm_build="$llvm_src/release"

# Check if LLVM is already built
is_llvm_built() {
    [[ -f "$IMPORT/bin/clang" ]] && [[ -d "$llvm_build" ]] || return 1
    return 0
}

# Build LLVM from scratch
build_llvm() {
    # Create directories
    mkdir -p "$build_dir"
    
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
        -S $llvm_src/llvm
        -DCMAKE_GENERATOR_PLATFORM=x64
        -DCMAKE_C_COMPILER="gcc-14"
        -DCMAKE_CXX_COMPILER="g++-14"
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=$IMPORT
        -G Ninja
        -DLLVM_ENABLE_ASSERTIONS=ON
        -DLLVM_ENABLE_PROJECTS='clang;lld;lldb'
        -DLLVM_TOOL_GOLD_BUILD=OFF
        -DLLVM_ENABLE_FFI=OFF
        -DLLVM_ENABLE_RTTI=ON
        -DLLVM_BINUTILS_INCDIR=/usr/include
        -DCLANG_DEFAULT_PIE_ON_LINUX=ON
        -DCLANG_CONFIG_FILE_SYSTEM_DIR=/etc/clang
        -DBUILD_SHARED_LIBS=ON
        -DLLDB_ENABLE_PYTHON=OFF
        -DLLVM_BUILD_LLVM_DYLIB=OFF
        -DLLVM_LINK_LLVM_DYLIB=OFF
        -DLLVM_TARGETS_TO_BUILD='host;X86;AArch64' # so what are these, then?  if we only get 1 set of libs installed?  is INSTALL somet other comman for other toolchain sdks?
        -DCLANG_DEFAULT_CXX_STDLIB=libstdc++
        -DCMAKE_C_FLAGS="-m64"
        -DCMAKE_CXX_FLAGS="-m64"
    )

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