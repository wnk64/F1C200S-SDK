#!/bin/bash
set -euo pipefail
P=/home/wnk/tk032f8004-panel
K=/home/wnk/LicheePi_Nano/linux_musb_clean_ep1_20260811
D=/home/wnk/LicheePi_Nano/third_party/rtl8723ds/f1c200s
CROSS=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-
OUT="$P/build-lg4573-timing"
cd "$K"
test -f .config && test -f vmlinux && test -f System.map && test -f arch/arm/boot/zImage
test "$(make -s ARCH=arm CROSS_COMPILE="$CROSS" LOCALVERSION= kernelrelease)" = 5.7.1
mkdir -p "$OUT"
cd "$P"
for spec in baseline:suniv-f1c200s-newboard-display.dts candidate:suniv-f1c200s-newboard-lg4573-timing.dts; do
    name=${spec%%:*}
    source=${spec#*:}
    "${CROSS}gcc" -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
        -I "$D/newboard_rtl8723bs_20260930" -I "$D/newboard_20260930" \
        -I "$K/arch/arm/boot/dts" -I "$K/include" \
        "$source" -o "$OUT/$name.preprocessed.dts"
    "$K/scripts/dtc/dtc" -I dts -O dtb -o "$OUT/$name.dtb" \
        "$OUT/$name.preprocessed.dts" 2> "$OUT/$name.dtc.log"
    "$K/scripts/dtc/dtc" -s -I dtb -O dts -o "$OUT/$name.sorted.dts" \
        "$OUT/$name.dtb" 2> "$OUT/$name.decode.log"
done
test "$(md5sum "$OUT/baseline.dtb" | cut -d ' ' -f1)" = 50499cb0527d269e0da7e436c092f643
set +e
diff -u "$OUT/baseline.sorted.dts" "$OUT/candidate.sorted.dts" > "$OUT/timing.diff"
rc=$?
set -e
test "$rc" = 1
cat "$OUT/timing.diff"
md5sum "$OUT/baseline.dtb" "$OUT/candidate.dtb"
