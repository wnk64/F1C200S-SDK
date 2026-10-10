# Worktree Change Log

## 2026-10-10 planned / Restore relocated menu helper
Baseline: project e2b77b0; GUI1984410ae0c932b8cdb14cd92b8f9128ee3ca744264de69c716a941809e602d1 unchanged. connections.c resolves connections-helper-ip relative to /proc/self/exe; HID SD directory lacks that executable. Stable helper exists under menu_graphics_boot_20261003; SHA8129fa341b7c9eca649c5c73abca3fd12f1566afe6bf1beab9ea92fe991127a8.
Action: copy existing unchanged helper into /root/roms/candidates/bluetooth_hid_20261010/connections-helper-ip, verify checksum and executable flag, run wifi-list/bt-list/wifi-scan/bt-scan through candidate path. Original remains untouched; remove only added candidate file to undo. No rebuild, kernel/network/bond/source changes. Bluetooth BLE startup issue separate, pending. Cold boots 0; no stable acceptance claim.
Deployment: planned record commit ccd7cbb; candidate helper copied via SSH control from existing board path, chmod700, SHA256 matched8129fa34...127a8; sync complete, persistent on SD. No overwritten file, original helper and UI retained.
Tests: wifi-list/wifi-scan and bt-list/bt-scan all returned DEVICE rows and DONE using exact new menu helper path. Wi-Fi associated wnk641_2.4G10.0.0.234, gateway ping2/2 loss0%, SSH healthy, supervisorPID269 still running. No GUI restart required since next invocation resolves the same previously missing pathname. Physical GUI interaction and3coldboots pending; BLE keyboard encryption/pairing issue not claimed fixed. No temporary helper scripts created.

## 2026-10-10 pending / BLE keyboard security diagnosis
Baseline: source 5801eef; running Linux 5.7.1 #250 unchanged. Xbox menu controls user-confirmed; keyboard presses produced zero input events.
Evidence: keyboard Paired=true in BlueZ but no LongTermKey section on disk; kernel security requested but not available; bt_io_set BT_SECURITY Invalid argument; LE reconnect timeouts. Do not log secret keys.
Hypothesis: audio-backend starts bluetoothd asynchronously then hciconfig up before HCI_MGMT initialization; legacy power-up bypasses SMP registration. Validate using BlueZ Powered=false/true, reconnect Xbox and keyboard. No service/source/kernel replacement yet.
Protected dependencies: Wi-Fi/SSH, existing Xbox bond and audio processes; adapter toggle briefly disconnects Bluetooth only. Restore Powered=true and reconnect existing devices. Cold boots 0; candidate remains pending.
Result: BlueZ Powered=false/true succeeded; cmac(aes) appeared in /proc/crypto immediately, previously absent. No new security-unavailable messages after toggle. Both devices currently disconnected; Xbox reconnect Failed, keyboard Connect NoReply. Requested physical wake/pairing mode; no bonds deleted, no kernel/service files changed. Adapter remains Powered=true; bluetoothd/bluealsa unchanged. MemAvailable37580kB/CmaFree17548kB. audio-backend source/runtime SHAba68631d4db5fae2ebdf618d00aa79bf9eac226b2acc3618657ed1cb3a8e5bae matches. Persistent startup correction and keyboard key verification pending.

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
