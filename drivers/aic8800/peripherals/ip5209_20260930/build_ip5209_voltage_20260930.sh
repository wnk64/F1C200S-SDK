#!/bin/sh
set -eu
KERNEL=/home/wnk/LicheePi_Nano/linux_musb_clean_ep1_20260811
DRIVER=/home/wnk/aic8800_ugreen_v14_20260919/peripherals/ip5209_20260930
PREFIX=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-
cd "$KERNEL"
test -f .config && test -f vmlinux && test -f System.map && test -f arch/arm/boot/zImage
test "$(make ARCH=arm CROSS_COMPILE="$PREFIX" LOCALVERSION= kernelrelease)" = 5.7.1
set +e
make -C "$KERNEL" ARCH=arm CROSS_COMPILE="$PREFIX" LOCALVERSION= M="$DRIVER" -j8 modules >/tmp/f1-ip5209-voltage-0930.log 2>&1
rc=$?
printf '%s\n' "$rc" >/tmp/f1-ip5209-voltage-0930.rc
exec bash
