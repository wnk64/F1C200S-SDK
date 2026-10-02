#!/bin/bash
set -euo pipefail
K=/home/wnk/LicheePi_Nano/linux_musb_clean_ep1_20260811
D=/home/wnk/LicheePi_Nano/third_party/rtl8723ds
CROSS=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-
build() {
    for file in .config vmlinux System.map arch/arm/boot/zImage; do test -f "$K/$file" || return; done
    git -C "$K" diff --quiet || return
    git -C "$K" diff --cached --quiet || return
    test "$(make -s -C "$K" ARCH=arm CROSS_COMPILE="$CROSS" LOCALVERSION= kernelrelease)" = 5.7.1 || return 1
    make -C "$K" ARCH=arm CROSS_COMPILE="$CROSS" LOCALVERSION= -j8 zImage modules || return
    bash "$D/f1c200s/newboard_rtl8723bs_20260930/build.sh"
}
set +e
build > /tmp/f1-bs-poll-0930.log 2>&1
rc=$?
printf '%s\n' "$rc" > /tmp/f1-bs-poll-0930.rc
exec bash
