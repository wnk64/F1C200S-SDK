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

## 2026-10-03 / Planned / Black playback startup instead of enlarged cover

用户要求进入播放前不显示放大的缩略图，允许黑屏。仅在WILIWILI_BOARD播放页取消surface上的封面图片和遮罩控件，背景设为纯黑；推荐/动态/详情的小封面仍保留，Windows预览不变。保护已有硬解、异步预读、UI自动隐藏和循环归零逻辑。原工具链增量构建并Git跟踪，不改kernel或投屏。

## 2026-10-03 / Planned / Reset UI progress on actual stream rewind

用户反馈循环结束后UI时间和进度继续递增。原Cedar循环在PSR_EOS执行CdxParserSeekTo(parser,0)，UI没有收到回绕通知。仅修改wiliwili专用HTTPS流seek适配与UI平台接口：从文件后半段实际seek到0时，经已有双向Unix数据报发送回绕事件，UI重置软件计时和进度。不依据duration自行猜循环，不改原投屏播放器。初始MP4索引读取若回到0也可产生重置，初始计时归零无害；不是解码输出PTS精确同步。

## 2026-10-03 / pending / Hidden black-strip candidate deployed

源码5358305，专用播放器SHA256 4af2c4480a0e7c84d5938afd597d7df00683bc92871bab6e833bd68ed3831a8a；原SDK增量make net-player成功，SSH部署哈希一致。仅重启本轮wiliwili应用，未重启板子、未修改投屏/驱动。隐藏时使用屏幕外x480、alpha255，显示时x0、alpha255。显示坐标修改仅作用UI层1，视频层0保持不变。实际黑条消失与触摸十秒切换待用户确认，状态pending。

## 2026-10-03 / Planned / Remove hidden controls from visible scanout

用户确认隐藏UI后控制条区域变黑。原方案只改层1全局alpha=0，而RGB层与视频层在同一硬件pipe，不能视为可靠的像素透明叠加。修正仅应用专用cedar_osd.c：控制条始终alpha255，隐藏时坐标移到x=480屏幕外，显示时恢复x=0；保持同一137600字节缓冲，不做整屏合成、不改kernel。验证隐藏时控制区域真实视频恢复，显示/十秒隐藏仍可用。未验证前不宣称修复。

## 2026-10-03 / pending / Hidden-control runtime and source-size result

源码8ab36ba/f3defe2/a7fcf2b；UI388b83f2、最终专用播放器f28f1791。原工具链make -C board_linux -j2 all net-player成功，SSH部署哈希一致，旧候选及投屏播放器未覆盖。
固定MV接口实测：fnval0/1即使qn32仍返回quality16/MP4，实际解码640x360；fnval16返回quality32，DASH H264/HEVC均852x480。不存在已发现的精确800x480源。当前未接DASH独立音频，不能宣称完成800x480播放。
控制层通过有界Unix数据报显示/隐藏，默认透明，中心区域逻辑坐标x250..550/y140..340点按显示，十秒无新点击且手指松开后隐藏。播放器初始化会覆盖alpha，已在OSD线程重新维持指定透明度；实际触摸/十秒隐藏待用户验证。
进度条按既有软件播放计时自动更新，非真实PTS反馈；原拖动/音量后端尚未接入，不宣称这些控件已实现。暂停目前SIGSTOP停止整个子进程，暂停时OSD线程也停止，故暂停期间隐藏命令暂不能应用，这是待修限制。
未验收稳定，冷启动0/3。下一步480p DASH双轨接入与实际PTS控制；暂保留640x360实际播放，不用缩放充当800x480。

## 2026-10-03 / Planned / 480-line source and auto-hidden playback controls

用户要求800x480视频及默认隐藏控制层，点击中心显示，10秒没有点击隐藏。B站API没有任意宽高参数；请求480p档qn=32、渐进MP4并以解码日志核对实际宽高，不将854x480宣称800x480，不下载转码补尺寸。
UI和专用播放器以有界Unix数据报传递显示/隐藏/更新命令，默认控制层alpha=0。保留原菜单、播放器和投屏不动。进度条按计时更新，未接真实PTS前标为软件计时，不宣称精确解码进度。

## 2026-10-03 / pending / OSD first test and bounded prefetch plan

源码da08463/f79ff73，UI174a16c7、专用播放器e4e546b8；真实B站640x360/25fps硬解与AAC音频同时运行。新增RGB565缓冲137600字节，内核读数显示层0和层1都启用，优先级分别0/1；用户确认无控制UI，因此不能以plane创建成功宣称显示成功。
用户确认声音正常后又出现偶发音频卡顿；帧日志多次有0.3至1.3秒间隔，循环边界有7.2秒间隔。系统MemAvailable约24MiB/CmaFree约9.5MiB，没有已知OOM证据。
下一假设：同步HTTPS Range下载阻塞解复用，导致两条输出队列耗尽。修改范围仅cdx_https读取适配器：单独下载线程、双256KiB缓存、总512KiB固定上限和提前读取下一块；不改音视频解码核心、内核、驱动或音频服务。记录HTTP等待与耗时，验证是否与实际帧间隙对应。
固定测试视频已按用户要求写入board_linux/TEST_PROTOCOL.md，BV1GJ411x7h7。当前仍pending，旧原播放器与UI保留，不推稳定。

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
