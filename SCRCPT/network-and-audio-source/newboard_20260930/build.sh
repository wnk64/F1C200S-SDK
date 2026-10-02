#!/bin/bash
set -euo pipefail

DRIVER=$(cd "$(dirname "$0")/../.." && pwd)
KERNEL=/home/wnk/LicheePi_Nano/linux_musb_clean_ep1_20260811
CROSS=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-
HERE="$DRIVER/f1c200s/newboard_20260930"
OUT="$HERE/artifacts"

for file in .config vmlinux System.map arch/arm/boot/zImage Module.symvers; do
    test -f "$KERNEL/$file" || { echo "Missing kernel prerequisite: $file"; exit 1; }
done
test "$(git -C "$KERNEL" rev-parse HEAD)" = d7e967964089727e2e12c55faa5dfec7f6b8ce6d
git -C "$KERNEL" diff --quiet
git -C "$KERNEL" diff --cached --quiet
git -C "$DRIVER" diff --quiet
git -C "$DRIVER" diff --cached --quiet
grep -qx 'CONFIG_LOCALVERSION=""' "$KERNEL/.config"
grep -qx '# CONFIG_LOCALVERSION_AUTO is not set' "$KERNEL/.config"
for option in CONFIG_MMC CONFIG_MMC_SUNXI CONFIG_PWRSEQ_SIMPLE CONFIG_CFG80211; do
    grep -qx "$option=y" "$KERNEL/.config"
done
RELEASE=$(make -s -C "$KERNEL" ARCH=arm CROSS_COMPILE="$CROSS" LOCALVERSION= kernelrelease)
test "$RELEASE" = 5.7.1
test "$(md5sum "$KERNEL/.config" | cut -d ' ' -f1)" = 21d91bd8757f7b2dbb20c8b0d5927a7a

mkdir -p "$OUT"
cd "$DRIVER"
# The generic platform block supplies little-endian/cfg80211 flags;
# explicit ARM/toolchain/KSRC arguments override its host defaults.
make ARCH=arm CROSS_COMPILE="$CROSS" LOCALVERSION= KSRC="$KERNEL" \
    CONFIG_PLATFORM_I386_PC=y -j8 modules
case "$(modinfo -F vermagic 8723ds.ko)" in
    '5.7.1 '*) ;;
    *) echo 'Module release mismatch'; exit 1 ;;
esac
modinfo -F alias 8723ds.ko | grep -q 'sdio:c\*v024CdD723\*'
cp 8723ds.ko "$OUT/"

"${CROSS}gcc" -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
    -I "$KERNEL/arch/arm/boot/dts" -I "$KERNEL/include" \
    "$HERE/suniv-f1c200s-newboard-rtl8723ds.dts" -o "$OUT/rtl8723ds.preprocessed.dts"
"$KERNEL/scripts/dtc/dtc" -I dts -O dtb -i "$KERNEL/arch/arm/boot/dts" \
    -o "$OUT/suniv-f1c200s-newboard-rtl8723ds.dtb" "$OUT/rtl8723ds.preprocessed.dts"
"$KERNEL/scripts/dtc/dtc" -I dtb -O dts \
    "$OUT/suniv-f1c200s-newboard-rtl8723ds.dtb" -o "$OUT/rtl8723ds.decompiled.dts"
{
    git rev-parse HEAD
    git -C "$KERNEL" rev-parse HEAD
    "${CROSS}gcc" --version | head -1
    modinfo -F vermagic "$OUT/8723ds.ko"
    md5sum "$OUT/8723ds.ko" "$OUT/suniv-f1c200s-newboard-rtl8723ds.dtb"
    sha256sum "$OUT/8723ds.ko" "$OUT/suniv-f1c200s-newboard-rtl8723ds.dtb"
} > "$OUT/manifest.txt"
echo 'RTL8723DS candidate built; nothing deployed.'
