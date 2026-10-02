#!/bin/bash
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
KERNEL=/home/wnk/LicheePi_Nano/linux_musb_clean_ep1_20260811
DRIVER=$(cd "$HERE/../.." && pwd)
CROSS=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-
OUT="$HERE/bluetooth-artifacts"
python3 "$HERE/bluetooth/verify-firmware.py" "$HERE/bluetooth/firmware"

for file in .config vmlinux System.map arch/arm/boot/zImage; do
    test -f "$KERNEL/$file" || { echo "Missing kernel prerequisite:$file"; exit 1; }
done
git -C "$KERNEL" diff --quiet
git -C "$KERNEL" diff --cached --quiet
git -C "$DRIVER" diff --quiet
git -C "$DRIVER" diff --cached --quiet
grep -qx 'CONFIG_LOCALVERSION=""' "$KERNEL/.config"
grep -qx '# CONFIG_LOCALVERSION_AUTO is not set' "$KERNEL/.config"
for option in SERIAL_DEV_BUS SERIAL_DEV_CTRL_TTYPORT BT_HCIUART_SERDEV BT_HCIUART_3WIRE BT_HCIUART_RTL; do
    grep -qx "CONFIG_$option=y" "$KERNEL/.config"
done
grep -qx 'CONFIG_BT_HCIUART=m' "$KERNEL/.config"
test "$(make -s -C "$KERNEL" ARCH=arm CROSS_COMPILE="$CROSS" LOCALVERSION= kernelrelease)" = 5.7.1

make -C "$KERNEL" ARCH=arm CROSS_COMPILE="$CROSS" LOCALVERSION= -j8 zImage modules
make -C "$KERNEL" ARCH=arm CROSS_COMPILE="$CROSS" LOCALVERSION= M="$HERE/bluetooth" -j8 modules
# Build WLAN against the same configuration without the older Wi-Fi-only gate.
make -C "$DRIVER" ARCH=arm CROSS_COMPILE="$CROSS" LOCALVERSION= KSRC="$KERNEL" CONFIG_PLATFORM_I386_PC=y -j8 modules

mkdir -p "$OUT"
"${CROSS}gcc" -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
    -I "$HERE" -I "$KERNEL/arch/arm/boot/dts" -I "$KERNEL/include" \
    "$HERE/suniv-f1c200s-newboard-rtl8723ds-bt.dts" -o "$OUT/rtl8723ds-bt.preprocessed.dts"
"$KERNEL/scripts/dtc/dtc" -I dts -O dtb -i "$KERNEL/arch/arm/boot/dts" \
    -o "$OUT/suniv-f1c200s-newboard-rtl8723ds-bt.dtb" "$OUT/rtl8723ds-bt.preprocessed.dts"
cp "$KERNEL/arch/arm/boot/zImage" "$KERNEL/.config" "$KERNEL/vmlinux" "$KERNEL/System.map" "$OUT/"
cp "$HERE/bluetooth/hci_uart.ko" "$KERNEL/drivers/bluetooth/btrtl.ko" "$DRIVER/8723ds.ko" "$OUT/"
cp "$KERNEL/drivers/usb/musb/sunxi.ko" "$KERNEL/drivers/usb/phy/phy-generic.ko" "$OUT/"
cp "$KERNEL/drivers/staging/media/sunxi/cedar/ve/cedar_ve.ko" \
    "$KERNEL/drivers/staging/media/sunxi/cedar/ion/sunxi_ion_core.ko" "$OUT/"
mkdir -p "$OUT/rtl_bt"
cp "$HERE/bluetooth/firmware/rtl8723ds_fw.bin" "$HERE/bluetooth/firmware/rtl8723ds_config.bin" "$OUT/rtl_bt/"
cp "$HERE/load-bluetooth.sh" "$HERE/load-wifi.sh" "$OUT/"
for module in "$OUT"/*.ko; do
    case "$(modinfo -F vermagic "$module")" in
        '5.7.1 '*) ;;
        *) echo "Module release mismatch:$module"; exit 1 ;;
    esac
done
modinfo -F alias "$OUT/hci_uart.ko" | grep -q 'realtek,rtl8723ds-bt'
{
    git -C "$KERNEL" rev-parse HEAD
    git -C "$DRIVER" rev-parse HEAD
    "${CROSS}gcc" --version | head -1
    for module in "$OUT"/*.ko; do
        printf '%s: ' "$(basename "$module")"
        modinfo -F vermagic "$module"
    done
    md5sum "$OUT"/zImage "$OUT"/*.ko "$OUT"/*.dtb "$OUT"/rtl_bt/*.bin
    sha256sum "$OUT"/zImage "$OUT"/*.ko "$OUT"/*.dtb "$OUT"/rtl_bt/*.bin
} > "$OUT/manifest.txt"
echo 'Bluetooth/WLAN candidate built; nothing deployed.'
