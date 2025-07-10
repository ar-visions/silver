#!/bin/sh
mkdir -p debug
mkdir -p debug/lib
PROJECT_PATH=/src/A BUILD_PATH=/src/A/debug DIRECTIVE=lib PROJECT=A sh ./headers.sh

