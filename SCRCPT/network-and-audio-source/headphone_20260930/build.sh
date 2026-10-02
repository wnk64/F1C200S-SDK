#!/bin/bash
set -euo pipefail
KERNEL=/home/wnk/LicheePi_Nano/linux_musb_clean_ep1_20260811
HERE=$(cd "$(dirname "$0")" && pwd)
CROSS=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-
for file in .config vmlinux System.map arch/arm/boot/zImage; do
    test -f "$KERNEL/$file"
done
git -C "$KERNEL" diff --quiet
git -C "$KERNEL" diff --cached --quiet
test "$(make -s -C "$KERNEL" ARCH=arm CROSS_COMPILE="$CROSS" LOCALVERSION= kernelrelease)" = 5.7.1
mkdir -p "$HERE/artifacts"
"${CROSS}gcc" -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
    -I "$KERNEL/arch/arm/boot/dts" -I "$KERNEL/include" \
    "$KERNEL/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano-headphone.dts" \
    -o "$HERE/artifacts/headphone.preprocessed.dts"
"$KERNEL/scripts/dtc/dtc" -I dts -O dtb \
    -o "$HERE/artifacts/suniv-f1c100s-licheepi-nano-headphone.dtb" "$HERE/artifacts/headphone.preprocessed.dts"
md5sum "$HERE/artifacts/suniv-f1c100s-licheepi-nano-headphone.dtb"
