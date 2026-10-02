## 2026-10-02 / Planned / Progressive feed, QR entry and exit
Baseline: 3a71367, user-confirmed touch navigation; bfdd34e7 runtime SHA256.
Files: windows_lvgl/src/main.cpp, header_cookies helper/tests, launch-with-menu.sh, phase2-record.md.
Hypothesis: metadata is blocked behind all eight serial covers; publish metadata first and load four visible covers.
QR: add missing board Mine entry, case-insensitive cookie headers, white quiet zone; human approval required.
Exit: close board display/font/player and terminate detached workers before C++ global teardown; return to existing menu.
Protected: existing kernel/drivers/network/BT/Miracast/menu binaries; acceptance pending, cold 0/3.

# Worktree Change Log

Worktree: /home/wnk/Desktop/F1c200S_SOFT/LVGL/wiliwili_lvgl
Component: LVGL wiliwili Linux application port
Base: 39695b1; full current application source; no previous ARM artifact
Purpose: link original LVGL UI against a bounded Linux fbdev/input/HTTPS platform.

## 2026-10-02 / Planned / First ARM UI port
Files: board_linux, windows_lvgl/src/main.cpp, change records.
Protected: kernel, DTB, WLAN, AIC, Bluetooth, Miracast, original menu/player binaries.
Toolchain: relocated existing Buildroot GCC6.4 and matching sysroot, no SDK replacement.
Rollback: stop isolated new candidate; existing board menu/player remain unchanged.
Tests: coordinate/bounds/color compatibility; ARM link; isolated finite-time display after authorized cast exit.
Acceptance: pending; cold boot cycles 0/3. No video-playback acceptance implied.
