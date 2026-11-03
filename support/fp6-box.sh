#!/bin/bash
# the phone's tree here: runs a command as the Fairphone's root
# does, arm64 under qemu, from what the installer ships. Example:
#   support/fp6-box.sh sh -c 'cd /src/silver && silver --build phone'
# lavapipe (optional): Ubuntu arm64 mesa-vulkan-drivers in $B/lvp
# needs: qemu-aarch64 (static) at $QEMU; platform/native/build/fp6-box
# (a copy of unshare: apparmor's silver-host profile gives it userns)
R=/src/silver
B=${BOX:-$R/install/tmp/fp6-box}
QEMU=${QEMU:-$B/qemu-aarch64}
[ -x "$QEMU" ] || { echo "no qemu-aarch64 at $QEMU"; exit 1; }
[ -x $R/platform/native/build/fp6-box ] ||
    cp /usr/bin/unshare $R/platform/native/build/fp6-box
if [ "$FP6_BOX_IN" != 1 ]; then
    FP6_BOX_IN=1 BOX=$B QEMU=$QEMU exec $R/platform/native/build/fp6-box \
        -rmpf --mount-proc=/proc "$0" "$@"
fi
set -e
FP=$R/platform/fp6
W=$B/work
N=$B/new
mkdir -p $N $W
mount -t tmpfs none $N
ov() { # target, lower folders, name of its writable layer
    mkdir -p $W/$3/up $W/$3/wk "$1"
    mount -t overlay overlay \
        -o lowerdir=$2,upperdir=$W/$3/up,workdir=$W/$3/wk "$1"
}
# the box alone: Mesa's software Vulkan (the phone has its GPU's)
lvp=$B/lvp
[ -d $lvp ] && ov $N $lvp:$FP/sysroot root || ov $N $FP/sysroot root
mkdir -p $N$R
ov $N$R $R src
# the phone has silver's tracked files, not these
for d in platform checkout; do mount -t tmpfs none $N$R/$d; done
[ -L $N$R/install ] && rm -f $N$R/install
mkdir -p $N$R/install
mount -t tmpfs none $N$R/install
I=$N$R/install
ov $I/build $FP/build build
ov $I/lib $FP/lib lib
ov $I/bin $FP/bin bin
ov $I/include $FP/include:$R/install/include include
ov $I/share $FP/share share
# as the installer: the shares of the modules built for the phone
for p in $FP/build/silver-*.product; do
    m=$(basename $p .product)
    [ -d $R/install/share/$m ] && [ $m != silver-os-bootstrap ] || continue
    mkdir -p $I/share/$m
    mount --bind -o ro $R/install/share/$m $I/share/$m
done
for p in $FP/build/silver-*.product; do
    s=$(basename $p .product); s=${s#silver-}
    [ -d $R/install/share/$s ] || continue
    mkdir -p $I/share/$s
    mount --bind -o ro $R/install/share/$s $I/share/$s
done
# the installer copies through links (cp -rL)
for l in $R/install/include/*; do
    [ -L "$l" ] && [ -e "$l" ] && [ ! -e $FP/include/$(basename $l) ] || continue
    rm -rf $I/include/$(basename $l)
    cp -rL "$l" $I/include/
done
mkdir -p $I/tmp
mount -t tmpfs none $I/tmp
mount --rbind /dev $N/dev
mount --rbind /sys $N/sys
mount -t proc proc $N/proc
mount -t tmpfs none $N/tmp
mount -t tmpfs none $N/run
mount -t binfmt_misc none $N/proc/sys/fs/binfmt_misc
M='\x7fELF\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\xb7\x00'
K='\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff'
K=$K'\xfe\xff\xff\xff'
echo ":aarch64:M::$M:$K:$QEMU:F" > $N/proc/sys/fs/binfmt_misc/register
# what the installer adds (os-bootstrap phone_rootfs)
sed -i -E "s|=$FP/sysroot/usr|=/usr|" \
    $N/usr/lib/aarch64-linux-gnu/pkgconfig/*.pc $N/usr/share/pkgconfig/*.pc
sed -i "s|$FP|$R/install|g" $I/lib/pkgconfig/*.pc $I/share/pkgconfig/*.pc
mkdir -p $N/etc/ssl/certs
cp /etc/ssl/certs/ca-certificates.crt $N/etc/ssl/certs/
printf '[safe]\n\tdirectory = *\n' > $N/etc/gitconfig
# the phone's modem writes its own; here this computer's
rm -f $N/etc/resolv.conf
cp -L /etc/resolv.conf $N/etc/resolv.conf
for p in $I/build/silver-*.product; do
    ln -sfn $R/install/build/$(basename $(readlink $p)) $p
done
VK=
[ -d $lvp ] && VK=VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json
exec chroot $N /usr/bin/env -i HOME=/root TERM=dumb $VK \
    PATH=$R/install/bin:/usr/bin:/bin "$@"
