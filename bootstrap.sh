#!/usr/bin/env bash
set -e

if [ -z "$IMPORT" ]; then
    IMPORT="$(realpath "$(dirname "$0")")"
fi

# install Xcode tools (base clang and sdk)
if [[ "$OSTYPE" == "darwin"* ]]; then
    if ! command -v clang >/dev/null 2>&1 && ! command -v gcc >/dev/null 2>&1; then
        echo "🛠️  macOS command line tools not found. Installing..."
        xcode-select --install || true
        until command -v clang >/dev/null 2>&1 || command -v gcc >/dev/null 2>&1; do
            sleep 2
        done
    fi
fi

# lets make sure silver bins come first
export PATH=$IMPORT/bin:$PATH

case "$OSTYPE" in
  darwin*)  SHLIB_EXT="dylib" ;;  # macOS
  linux*)   SHLIB_EXT="so" ;;     # Linux / WSL
  msys*|cygwin*|win*) SHLIB_EXT="dll" ;;  # Windows or Git-Bash
  *)        SHLIB_EXT="so" ;;     # fallback
esac

export SHLIB_EXT

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

    CC=gcc ./configure --prefix=$IMPORT --enable-shared --with-ensurepip=install
    make -j$(sysctl -n hw.ncpu)
    make install


    cd "$IMPORT/checkout"
    curl -LO https://github.com/swig/swig/archive/refs/tags/v4.1.1.tar.gz
    tar -xf v4.1.1.tar.gz
    cd swig-4.1.1

    # point it explicitly to your Python
    export PATH="$IMPORT/bin:$PATH"
    export LD_LIBRARY_PATH="$IMPORT/lib:$LD_LIBRARY_PATH"

    ./autogen.sh || true
    ./configure \
    --prefix=$IMPORT \
    --with-python=$IMPORT/bin/python3 \
    --without-pcre  # optional; system PCRE is usually fine

    make -j$(nproc)
    make install

fi

(cd $IMPORT && python3 import.py "$@")

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

(cd $IMPORT && python3 gen.py "$@")
