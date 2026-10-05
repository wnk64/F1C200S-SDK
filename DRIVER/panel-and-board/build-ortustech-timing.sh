#!/bin/bash
set -euo pipefail
ROOT=/home/wnk/Desktop/F1c200S_SOFT
P="$ROOT/DRIVER/panel-and-board"
K="$ROOT/KERNEL/linux-5.7.1"
D="$ROOT/SCRCPT/network-and-audio-source"
CROSS=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-
OUT="$P/build-ortustech-timing"
cd "$K"
test -f .config && test -f vmlinux && test -f System.map && test -f arch/arm/boot/zImage
test "$(md5sum arch/arm/boot/zImage | cut -d ' ' -f1)" = bffc71e8bcd28ef7f1f18616af72ab38
test "$(make -s ARCH=arm CROSS_COMPILE="$CROSS" LOCALVERSION= kernelrelease)" = 5.7.1
mkdir -p "$OUT"
cd "$P"
for spec in baseline:suniv-f1c200s-newboard-lg4573-timing.dts candidate:suniv-f1c200s-newboard-ortustech-timing.dts; do
    name=${spec%%:*}
    source=${spec#*:}
    "${CROSS}gcc" -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
        -I "$D/newboard_rtl8723bs_20260930" -I "$D/newboard_20260930" \
        -I "$K/arch/arm/boot/dts" -I "$K/include" \
        "$source" -o "$OUT/$name.preprocessed.dts"
    "$K/scripts/dtc/dtc" -I dts -O dtb -o "$OUT/$name.dtb" "$OUT/$name.preprocessed.dts" 2> "$OUT/$name.dtc.log"
    "$K/scripts/dtc/dtc" -s -I dtb -O dts -o "$OUT/$name.sorted.dts" "$OUT/$name.dtb" 2> "$OUT/$name.decode.log"
done
test "$(md5sum "$OUT/baseline.dtb" | cut -d ' ' -f1)" = 7018246d33a30340690f9a4edd6406e7
set +e
diff -u "$OUT/baseline.sorted.dts" "$OUT/candidate.sorted.dts" > "$OUT/timing.diff"
rc=$?
set -e
test "$rc" = 1
cat "$OUT/timing.diff"
sha256sum "$OUT/baseline.dtb" "$OUT/candidate.dtb"
