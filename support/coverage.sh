#!/bin/sh
# aether/silver coverage of a module's expects
S="$(cd "$(dirname "$0")/.." && pwd)"
C="$S/install/coverage"
M="${1:-features}"
X=so
[ "$(uname)" = Darwin ] && X=dylib
cd "$S" || exit 1
rm -rf "$C/prof" && mkdir -p "$C/prof"
# %c: counters stay on disk, silver execs the test app
export LLVM_PROFILE_FILE="$C/prof/%c%p-%m.profraw"
# from scratch: its checkouts, their builds and its products go
"$C/silver" --uninstall "$M" || echo "coverage: $M uninstall failed"
"$C/silver" --test "$M" || echo "coverage: $M tests failed"
unset LLVM_PROFILE_FILE
"$S/install/bin/llvm-profdata" merge -sparse "$C"/prof/*.profraw -o "$C/silver.profdata" || exit 1
OBJ="$C/silver -object $C/libaether.$X -object $C/libsilver.$X"
SRC="src/aether.c src/silver.c"
"$S/install/bin/llvm-cov" report $OBJ -instr-profile "$C/silver.profdata" $SRC
"$S/install/bin/llvm-cov" show $OBJ -instr-profile "$C/silver.profdata" -format=html -output-dir "$C/html" $SRC
# the map orbiter shows: the last coverage run's
"$S/install/bin/llvm-cov" export $OBJ -instr-profile "$C/silver.profdata" -format=lcov $SRC > "$S/install/tmp/coverage.lcov"
# branch totals without the check macros' failure sides
"$S/install/bin/llvm-cov" export $OBJ -instr-profile "$C/silver.profdata" -format=text $SRC > "$C/coverage.json"
python3 "$S/support/coverage-branches.py" "$C/coverage.json" $SRC
echo "coverage: $C/html/index.html, map $S/install/tmp/coverage.lcov"
