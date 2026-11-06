#!/bin/sh
# javac on a file list (one command line cannot hold them all), then the jar
out=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
dir=$2
list=$3
shift 3
rm -rf "$dir"
mkdir -p "$dir"
javac -d "$dir" "$@" @"$list" || exit 1
cd "$dir" && jar cf "$out" .
