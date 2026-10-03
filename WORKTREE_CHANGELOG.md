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

## 2026-10-03 / Planned / Small independent RGB playback controls

基线48577e6：专用HTTPS网络播放器已读取B站360p MP4并硬解90度旋转；播放器与UI同时显示尚未验收。旧UI程序9e43a1a7及投屏player-aud-resume保留。
上一轮UI没有解码子进程：用户配置为125%播放速度，后端仅支持100%，因此prepare拒绝；先将板端支持范围明确限制为正常速度，不宣称已实现倍速或拖动。
计划仅修改wiliwili应用和应用专用播放器适配模块：视频保持DRM层0；控制条用DRM层1、RGB565、86x800小缓冲，新增CMA约136KiB；直接取现有fb0控制条像素，eventfd只在UI刷新后通知复制，不分配新整屏UI缓冲、不持续轮询整屏。退出恢复原fbdev层并释放新增缓冲。
保持kernel、DTB、原投屏播放器、无线和蓝牙服务不变。验证持续真实帧提交、播放/暂停/返回、GUI与播放器同时运行的RAM/CPU、CMA退出回收、菜单恢复。候选状态pending，三冷0/3。

## 2026-10-03 / Planned / Bilibili HTTPS hardware playback

基线：原工作树 39806ed，完整 LVGL/curl 源码与匹配产物保留，未新建工作树。
证据：B站 API 返回360p、带音频MP4；现有播放器在解复用前报不支持https scheme。
计划：仅增加wiliwili专用CdxStream HTTPS Range适配器，256KiB有界缓存，使用已有libcurl/OpenSSL校验证书；重用当前cedar-player完整源码，在应用build-arm内生成独立播放器，不覆盖投屏播放器、解码库、kernel或驱动。板端UI连接独立子进程，正确停止/回收；B站优先请求低清H264渐进MP4。
回退：保留板端原wiliwili程序9e43a1a7，停止新候选后使用原菜单入口。
测试：Range/seek与真实MP4解析、解码帧持续提交、菜单退出恢复、内存；三次冷启动与蓝牙/投屏回归仍待验收，禁止称稳定。

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
## 2026-10-02 图片逐张更新候选结果

修改范围仅wiliwili应用：main.cpp逐张封面完成队列、动态独立缓存及重新进入刷新、过期完成校验、动态详情缓存隔离、取消请求释放加载状态；platform_linux.cpp识别generic ft5x06；launch-with-menu.sh记录真实退出码。

源码提交2aa87d3至87b6315；原目录make -j2，tmux构建rc=0，产物SHA256 9e43a1a74c60dbf67095dd1002cd1e1703393edd5adf400736afddccedc97f73。板端逐张日志与截图通过，4轮快速切页测试未复现崩溃，正常退出rc=0并返回菜单。RSS约7.4MiB；动态4张缩略图约35KiB。先前用户报告的崩溃原因仍未捕获，未断言OOM。冷启动验收0/3。回退67ac5cb原完整源码，保护内核、驱动、投屏和联网。未新建工作树、未推送稳定归档。
