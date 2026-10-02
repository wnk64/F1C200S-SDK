#!/bin/bash
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
K=/home/wnk/LicheePi_Nano/linux_musb_clean_ep1_20260811
D=$(cd "$HERE/../.." && pwd)
BT="$D/f1c200s/newboard_20260930/bluetooth"
CROSS=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-
OUT="$HERE/artifacts"
for file in .config vmlinux System.map arch/arm/boot/zImage Module.symvers; do test -f "$K/$file"; done
git -C "$K" diff --quiet
git -C "$K" diff --cached --quiet
git -C "$D" diff --quiet
git -C "$D" diff --cached --quiet
grep -qx 'CONFIG_LOCALVERSION=""' "$K/.config"
grep -qx '# CONFIG_LOCALVERSION_AUTO is not set' "$K/.config"
grep -qx 'CONFIG_RTL8723BS=m' "$K/.config"
grep -qx 'CONFIG_SERIAL_DEV_BUS=y' "$K/.config"
test "$(make -s -C "$K" ARCH=arm CROSS_COMPILE="$CROSS" LOCALVERSION= kernelrelease)" = 5.7.1
python3 "$HERE/verify-firmware.py" "$HERE/firmware"
# Rebuild only the enabled BS driver and the staged H5 adapter.
make -C "$K" ARCH=arm CROSS_COMPILE="$CROSS" LOCALVERSION= -j8 drivers/staging/rtl8723bs/
make -C "$K" ARCH=arm CROSS_COMPILE="$CROSS" LOCALVERSION= -j8 M="$BT" modules
mkdir -p "$OUT"
cp "$K/drivers/staging/rtl8723bs/r8723bs.ko" "$BT/hci_uart.ko" "$K/drivers/bluetooth/btrtl.ko" "$OUT/"
"${CROSS}gcc" -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
    -I "$D/f1c200s/newboard_20260930" -I "$K/arch/arm/boot/dts" -I "$K/include" \
    "$HERE/suniv-f1c200s-newboard-rtl8723bs.dts" -o "$OUT/rtl8723bs.preprocessed.dts"
"$K/scripts/dtc/dtc" -I dts -O dtb \
    -o "$OUT/suniv-f1c200s-newboard-rtl8723bs.dtb" "$OUT/rtl8723bs.preprocessed.dts"
cp "$K/arch/arm/boot/zImage" "$K/.config" "$K/vmlinux" "$K/System.map" "$OUT/"
cp "$K/drivers/usb/musb/sunxi.ko" "$K/drivers/usb/phy/phy-generic.ko" "$OUT/"
cp "$K/drivers/staging/media/sunxi/cedar/ve/cedar_ve.ko" "$K/drivers/staging/media/sunxi/cedar/ion/sunxi_ion_core.ko" "$OUT/"
cp -a "$HERE/firmware/rtl_bt" "$HERE/firmware/rtlwifi" "$OUT/"
cp "$HERE/load-wifi.sh" "$HERE/load-bluetooth.sh" "$OUT/"
for module in "$OUT"/*.ko; do
    case "$(modinfo -F vermagic "$module")" in '5.7.1 '*) ;; *) exit 1 ;; esac
done
modinfo -F alias "$OUT/r8723bs.ko" | grep -q 'v024CdB723'
modinfo -F alias "$OUT/hci_uart.ko" | grep -q 'realtek,rtl8723bs-bt'
{
    git -C "$K" rev-parse HEAD
    git -C "$D" rev-parse HEAD
    for module in "$OUT"/*.ko; do printf '%s: ' "$(basename "$module")"; modinfo -F vermagic "$module"; done
    md5sum "$OUT"/zImage "$OUT"/*.ko "$OUT"/*.dtb "$OUT"/rtl_bt/* "$OUT"/rtlwifi/*
    sha256sum "$OUT"/zImage "$OUT"/*.ko "$OUT"/*.dtb "$OUT"/rtl_bt/* "$OUT"/rtlwifi/*
} > "$OUT/manifest.txt"
tar -cf "$OUT/deployment-candidate.tar" -C "$OUT" zImage r8723bs.ko hci_uart.ko btrtl.ko \
    sunxi.ko phy-generic.ko cedar_ve.ko sunxi_ion_core.ko suniv-f1c200s-newboard-rtl8723bs.dtb \
    rtl_bt rtlwifi load-wifi.sh load-bluetooth.sh manifest.txt
echo 'RTL8723BS candidate built, nothing deployed.'
