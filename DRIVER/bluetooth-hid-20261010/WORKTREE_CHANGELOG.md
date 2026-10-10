# Worktree Change Log

Worktree: /home/wnk/Desktop/F1c200S_SOFT
Component: external upstream HIDP/UHID modules
Base: project7bab3da; kernelb3d8ed3 Linux5.7.1#250
Purpose: Bluetooth keyboard/Xbox controller support without replacing kernel.

## 2026-10-10 planned
User supplies make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j8; preserve LOCALVERSION=5.7.1 exactly with LOCALVERSION= argument. Existing toolchain Linaro7.2.1 discovered /opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin.
Exact preflight .config3e4a7d09/vmlinuxeb0d95a0/System.map998a9129/zImagebffc71e8 matches existing#250 record; running5.7.1#250 confirmed SSH. Kernel sources tracked, no source/config changes.
Files: copied unmodified kernel net/bluetooth/hidp/{core.c,sock.c,hidp.h},drivers/hid/uhid.c into DRIVER/bluetooth-hid-20261010/src; externalMakefile/build script, runtime loader/tests; shared evdev gamepad mapping separately recorded.
Kernel core exports BT/HID dependencies. Module-only first under external staging skill; no rebuild zImage/DTB/rootfs. Old runtime unaffected when candidate modules removed after disconnect.
Dependencies: existing BlueZ input/hog, hciUART/8723BS, HID/input/evdev, LVGL controls; protected Wi-Fi/SSH/defaultBTaudio/cast/display. Module hashes/vermagic and runtime checks required before deployment.
Xbox model/firmware transport unknown until scan/pair; generic existingHID_MICROSOFT may suffice, vendor-specific extension not presumed.
Decision: pending true keyboard/controller pairing/reconnect/menu/game controls, Bluetooth audio coexistence and3coldboots. No stable acceptance claim.
