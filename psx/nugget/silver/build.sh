#!/bin/sh
# OpenBIOS for MIPS I with LLVM: our clang, lld and llvm-objcopy,
# then into the psx module's share folder. $1: the install root
set -e
install="$1"
here="$(cd "$(dirname "$0")" && pwd)"
export LLVM_BIN="$install/bin"
tools="CC=$here/mips-cc CXX=$here/mips-cc AR=$install/bin/llvm-ar"
tools="$tools PREFIX=$here/mips FORMAT=elf32-tradlittlemips"
make -C shell -j16 shell_data.o $tools
make -C openbios -j16 $tools
mkdir -p "$install/share/silver-psx"
cp openbios/openbios.bin "$install/share/silver-psx/openbios.bin"
