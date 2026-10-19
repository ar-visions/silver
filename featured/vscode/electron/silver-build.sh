#!/bin/sh
# Electron from this checkout's source on its Chromium, built in
# <install>/build/electron/electron; the dist lands in <install>/lib/electron
set -e
install=$1
root=$2
here=$(pwd)
depot=$root/checkout/chromium/tools/depot_tools
gn=$install/build/electron/electron
need_gb=70
export PATH="$depot:$PATH" DEPOT_TOOLS_UPDATE=0
export CHROMIUM_BUILDTOOLS_PATH=$gn/src/buildtools

# the first sync takes Chromium whole: refuse rather than fill the disk
if [ ! -d "$gn/src/third_party/blink" ]; then
    free_gb=$(df -Pk "$install" | awk 'NR == 2 { print int($4 / 1048576) }')
    if [ "$free_gb" -lt "$need_gb" ]; then
        echo "electron: Chromium needs $need_gb GB free to check out and build;"
        echo "electron: $free_gb GB free on the disk holding $install"
        exit 1
    fi
fi

# src/electron is this checkout (silver's clone and overlay), kept in step
mkdir -p "$gn/src"
rsync -a --delete --exclude node_modules "$here/" "$gn/src/electron/"
cd "$gn"
[ -f .gclient ] || gclient config --name src/electron --unmanaged \
    https://github.com/electron/electron
gclient sync --with_branch_heads --with_tags --no-history

cd src
gn gen out/Release --args='import("//electron/build/args/release.gn")
    override_electron_version="43.7.7"
    symbol_level=0 blink_symbol_level=0 v8_symbol_level=0'
autoninja -C out/Release electron electron:electron_dist_zip

rm -rf "$install/lib/electron"
mkdir -p "$install/lib/electron"
unzip -q out/Release/dist.zip -d "$install/lib/electron"
