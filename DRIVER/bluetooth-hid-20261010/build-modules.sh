#!/bin/sh
set -eu
SELF=$(CDPATH= cd "$(dirname "$0")" && pwd)
KERNEL=/home/wnk/Desktop/F1c200S_SOFT/KERNEL/linux-5.7.1
export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH
test "$(md5sum "$KERNEL/.config" | cut -d ' ' -f1)" = 3e4a7d093691315cb1df4fe21123a4ea
test "$(md5sum "$KERNEL/arch/arm/boot/zImage" | cut -d ' ' -f1)" = bffc71e8bcd28ef7f1f18616af72ab38
release=$(make -s -C "$KERNEL" ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- LOCALVERSION= kernelrelease)
test "$release" = 5.7.1
make -C "$KERNEL" ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- LOCALVERSION= -j8 M="$SELF/src" modules
for module in hidp uhid; do
    case "$(modinfo -F vermagic "$SELF/src/$module.ko")" in '5.7.1 '*) ;; *) exit 1;; esac
    sha256sum "$SELF/src/$module.ko"
done
