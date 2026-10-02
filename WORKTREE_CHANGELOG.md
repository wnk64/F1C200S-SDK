## 2026-10-02 / pending / ARM UI runtime and timer correction
Source: d57c4e9; runtime SHA256 99178e05ccdea1d7bc1d75b917fff4c389b2641fc2a6622be5c8d1cc5c67520e.
Launcher SHA256 4f7389a3bda2557c9fe5c690bb3efc7553d01f7d31ffb90ff070fa7531325fc1.
Build: sh board_linux/build-curl.sh; make -C board_linux -j2, existing SDK GCC6.4; rc=0, ARM926EJ-S/v5TEJ.
Tests: coordinate/color, JPEG corruption/bounds, cookie header sanitizer checks; board verified TLS/API GET, MD5/body cap, finite UI rc0; user confirmed touch navigation.
Feed: metadata757ms/covers3235ms; idle settings CPU1.01%, RSS6212KiB/PSS4973KiB, available25524KiB (not loading/video peak).
Exit: finite app exit restored menu; test F1_UI_TEST_MS6000 leaked into menu and caused repeat expiry. Launcher now unsets it; parent20718 environment has no timer.
Protected: no kernel/driver/WLAN/BT/Miracast binary change. Software time set from PC, no RTC write; private candidate CA. Cold0/3; account confirmation, keyboard and playback pending.

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

## 2026-10-02 图片逐张显示与动态刷新候选（计划）

基线：原工作树 candidate/f1c200s-fbdev-20261002，67ac5cb；完整源码和 build-arm 产物均在原目录。仅修改 wiliwili 应用，不修改内核、驱动、投屏及菜单服务。

假设：封面整批发布造成等待；动态重进未清空旧数据和分页偏移，视频序号复用造成旧封面串用。崩溃尚无退出码证据，不能归因于 OOM。

计划：有界单槽封面完成队列，主线程逐张解码和更新现存图片控件；请求代次和视频标识双校验；删除控件时解除注册；切换动态详情前释放旧图片；动态重新进入重置分页并刷新；启动器记录真实退出码。

构建：board_linux/make -j2，原 Buildroot GCC6 工具链，tmux 持久会话。先提交再部署应用候选；保留 Git 历史及基线产物。三次冷启动验收未完成，不推送稳定归档。
