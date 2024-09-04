#!/bin/bash

SCRIPT_DIR=$(dirname "$(realpath "$0")")
cd      $SCRIPT_DIR || exit 1
mkdir -p silver-build
cd       silver-build
mkdir -p checkout
mkdir -p install
cd       checkout

TARGET_DIR="A"

if [ -d "$TARGET_DIR" ]; then
    echo "Directory $TARGET_DIR already exists. Pulling latest changes..."
    cd "$TARGET_DIR" || exit 1
    #PULL_HASH_0=$(git rev-parse HEAD)
    #git pull || exit 1
    #PULL_HASH_1=$(git rev-parse HEAD)
    #if [ "$PULL_HASH_0" != "$PULL_HASH_1" ]; then
    #    rm -f silver-build/silver-token || exit 1
    #fi
else
    echo "cloning repository into $TARGET_DIR..."
    git clone https://github.com/ar-visions/A.git "$TARGET_DIR"
    if [ $? -ne 0 ]; then
        echo "clone failure"
        exit 1
    fi
    cd "$TARGET_DIR"
fi

mkdir -p silver-build
cd       silver-build

if [ ! -f "silver-token" ]; then
    BUILD_DIR=$(pwd)
    echo BUILD_DIR = $BUILD_DIR

    cmake -S .. -B . -DCMAKE_INSTALL_PREFIX=$BUILD_DIR/../../../install
    if [ $? -ne 0 ]; then
        echo "A gen failure"
        exit 1
    fi

    cmake --build .
    if [ $? -ne 0 ]; then
        echo "A build failure"
        exit 1
    fi

    cmake --install .
    if [ $? -ne 0 ]; then
        echo "A install failure"
        exit 1
    fi
    
    echo "im a token" >> silver-token # i only want this to happen if all 3 of those are exit 0
fi


