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

## 2026-10-10 runtime candidate
Source56afff3 externalmodules, bfbeca7Xbox/menu mapping. Build-modules uses originalLinaro7.2.1/top-level -j8/M= with LOCALVERSION=; kernelrelease5.7.1, modulevermagic5.7.1 mod_unload ARMv5 p2v8, no zImage/config changes.
HIDP SHA5ca3408aabea5ddf3e7d890d3703c38a3cbe4b6b4dda6df9cb8a09d3303562b3; UHIDbf22cdf15b8034c2e213851154d42a9e3bd41d2e8b829edd36bde7b3957ce11b; SCP matched, insmod both successful/dev/uhid10:239.
Xbox83:71:27:73:15:95 paired/trusted/connected, kernelmicrosoft0005:045e:02fd/0903, inputevent0/js0 registered. Warning unknown main item tag0x0 recorded; not yet shown harmful.30s rawprobe0changes, no physical button validation yet.
Keyboard reported by user bt keybord, not in firstscan; leave target choice pending, do not pair unknown nearby devices.
Planned nextdeploy: standaloneSD f1-menu-hid candidate, original GUIafdd7cd4 retained, supervisorUI path only. S34 loader beforeaudio uses SD loaded during earlierboot; runtime load already successful. InitialGUIcandidate1984410a SHAverified, existingfont copied toSD (VFAT no symlink).
Rollback: supervisorUI originalpath and remove newS34service; disconnect HID devices before rmmodhidp/uhid. No forcedmodule removal, no old source or user file deletion.
