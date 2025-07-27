#!/bin/sh
# invoke the llvm.sh (installs VS 2022 equivalent or builds LLVM if not present)

set -e

DEPS_DIR="$(dirname "$0")/deps"
echo "[silver] installing LLVM..."

if ! "$DEPS_DIR/llvm.sh"; then
    echo "[install] failed to fetch dependencies."
    exit 1
fi

echo "[install] dependencies built successfully."

#mkdir -p debug
#mkdir -p debug/lib
#PROJECT_PATH=/src/A BUILD_PATH=/src/A/debug DIRECTIVE=lib PROJECT=A sh ./headers.sh