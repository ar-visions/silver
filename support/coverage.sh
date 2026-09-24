#!/bin/sh
# aether/silver coverage of a module's expects
S="$(cd "$(dirname "$0")/.." && pwd)"
C="$S/install/coverage"
M="${1:-features}"
X=so
[ "$(uname)" = Darwin ] && X=dylib
cd "$S" || exit 1
rm -rf "$C/prof" && mkdir -p "$C/prof"
touch "$M/$M.ag"
# %c: counters stay on disk, silver execs the test app
LLVM_PROFILE_FILE="$C/prof/%c%p-%m.profraw" "$C/silver" --test "$M" || echo "coverage: $M tests failed"
"$S/install/bin/llvm-profdata" merge -sparse "$C"/prof/*.profraw -o "$C/silver.profdata" || exit 1
OBJ="$C/silver -object $C/libaether.$X -object $C/libsilver.$X"
SRC="src/aether.c src/silver.c src/aclang.cc"
"$S/install/bin/llvm-cov" report $OBJ -instr-profile "$C/silver.profdata" $SRC
"$S/install/bin/llvm-cov" show $OBJ -instr-profile "$C/silver.profdata" -format=html -output-dir "$C/html" $SRC
echo "coverage: $C/html/index.html"
