#!/usr/bin/env bash
set -e

silver="$(cd "$(dirname "$0")" && pwd)"
src="$(dirname "$silver")"
mkdir -p $silver/import
mkdir -p $silver/import/checkout
export SILVER_IMPORT="$silver/import"
BASH_FILE=""

# clone base dependencies in src directory if they do not exist
if [ ! -d "$src/A" ]; then
    git clone https://github.com/ar-visions/A
    ln -s $src/A $silver/import/checkout/A
fi
if [ ! -d "$src/ether" ]; then
    git clone https://github.com/ar-visions/ether
    ln -s $src/A $silver/import/checkout/ether
fi

# select profile file
if   [ -f "$HOME/.bash_profile" ]; then
    BASH_FILE="$HOME/.bash_profile"
elif [ -f "$HOME/.bashrc"       ]; then
    BASH_FILE="$HOME/.bashrc"
else
    BASH_FILE="$HOME/.bash_profile"
    touch "$BASH_FILE"
fi

# find instances of $SILVER_IMPORT [used in PATH] and SILVER_IMPORT= [env]
r_import='^[[:space:]]*export[[:space:]]+SILVER_IMPORT='
r_dbg='^[[:space:]]*export[[:space:]]+DBG='
r_src='^[[:space:]]*export[[:space:]]+SRC='
r_path='\$SILVER_IMPORT'
f="$(mktemp)"
path_set=0
src_set=0
importer_exporter=0

while IFS= read -r line; do
    if   [[ "$line" =~ $r_path   ]]; then
        path_set=1
    elif [[ "$line" =~ $r_import ]]; then
        importer_exporter=1
        # lets read what its set to, then we can compare
    elif [[ "$line" =~ $r_src ]]; then
        src_set=1 # avoid setting this again; the user has control after default case
    elif [[ "$line" =~ $r_dbg ]]; then
        todo=1
        # avoid multiple DBG exports, but only if we are setting Debug!
        #continue
    fi
    echo "$line" >> "$f"
done < "$BASH_FILE"

# 3. append IMPORT if it wasn't found
if [ "$path_set" -eq 0 ]; then
    echo >> "$f"
    echo "export PATH=\"\$SILVER_IMPORT/bin:\$PATH\"" >> "$f"
    echo "silver: updated PATH in $BASH_FILE"
fi

if [ "$src_set" -eq 0 ]; then
    echo >> "$f"
    # lets set SRC to the folder in which we reside, where 'silver' is ?
    echo "export SRC=\"$src\"" >> "$f"
    echo "silver: updated PATH in $BASH_FILE"
fi

if [ "$importer_exporter" -eq 0 ]; then
    echo >> "$f"
    echo "export SILVER_IMPORT=\"$SILVER_IMPORT\"" >> "$f"
    echo "silver: added SILVER_IMPORT in $BASH_FILE"
fi

# if we have arguments, then we export DBG="arg1,arg2"
if [ "$#" -gt 0 ]; then
    export DBG=$(IFS=, ; echo "$*")
    echo "export DBG=\"$DBG\"" >> "$f"
    echo "silver: added DBG=\"$DBG\" to $BASH_FILE"
fi

echo "silver: environment set"

# 4. move updated file back
mv "$f" "$BASH_FILE"

# 5. ensure dirs exist
mkdir -p "$SILVER_IMPORT/bin" "$SILVER_IMPORT/include" "$SILVER_IMPORT/lib"

make
