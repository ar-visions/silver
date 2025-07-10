#!/usr/bin/env bash
set -e

import="$(cd "$(dirname "$0")" && pwd)"
src="$(dirname "$import")"
mkdir -p $import
mkdir -p $import/checkout
mkdir -p $import/tokens
mkdir -p $import/lib
mkdir -p $import/include
mkdir -p $import/bin
export IMPORT="$import"
BASH_FILE=""

# select profile file
if   [ -f "$HOME/.bashrc" ]; then
    BASH_FILE="$HOME/.bashrc"
elif [ -f "$HOME/.bash_profile"       ]; then
    BASH_FILE="$HOME/.bash_profile"
else
    BASH_FILE="$HOME/.bash_profile"
    touch "$BASH_FILE"
fi

# find instances of $IMPORT [used in PATH] and IMPORT= [env]
r_import='^[[:space:]]*export[[:space:]]+IMPORT='
r_dbg='^[[:space:]]*export[[:space:]]+DBG='
r_path='\$IMPORT'
f="$(mktemp)"
path_set=0
dbg_set=0
importer_exporter=0

while IFS= read -r line; do
    if   [[ "$line" =~ $r_path   ]]; then
        path_set=1
    elif [[ "$line" =~ $r_import ]]; then
        importer_exporter=1
        # lets read what its set to, then we can compare
    elif [[ "$line" =~ $r_dbg ]]; then
        dbg_set=1
        line="export DBG=\"$DBG\""
        echo "import: updated DBG in $BASH_FILE"
    fi
    echo "$line" >> "$f"
done < "$BASH_FILE"

if [ "$importer_exporter" -eq 0 ]; then
    echo >> "$f"
    echo "export IMPORT=\"$IMPORT\"" >> "$f"
    echo "import: added IMPORT in $BASH_FILE"
fi

# 3. append IMPORT if it wasn't found
if [ "$path_set" -eq 0 ]; then
    echo >> "$f"
    echo "export PATH=\"\$IMPORT/bin:\$PATH\"" >> "$f"
    echo "import: updated PATH in $BASH_FILE"
fi

# insert DBG if not set
if [ "$dbg_set" -eq 0 ]; then
    echo >> "$f"
    echo "export DBG=\"$DBG\"" >> "$f"
    echo "import: added DBG=\"$DBG\" in $BASH_FILE"
fi

echo "import: environment set"

# 4. move updated file back
mv "$f" "$BASH_FILE"

# setup A symlink in checkout
mkdir -p checkout
cd checkout
[ -L A ] || ln -s ../../A

make
