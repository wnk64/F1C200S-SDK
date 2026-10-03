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

## 2026-10-03 / Planned / Audio-master pacing after seek

用户确认仍画面滞后。实测target184000ms，audio first179967833us，video preroll180120000us，丢97帧后first display184000000us，证明上一候选确实丢了目标之前的视频，不能再把所有问题归为旧关键帧。
原video按独立墙钟等帧，audio按ALSA队列输出，两者没有持续同步。专用适配增加音频已写入样本PTS减ALSA delay的播放时钟，结合单调时钟短期外推；视频在RequestPicture处按该时钟等待或丢弃滞后>100ms的已解码帧，取消原视频额外重复等待。只针对有音频且音频时钟有效的会话，无音频仍保留原节奏。记录A/V delta和丢帧计数，禁止以日志正常代替实际嘴型验收；保护裁剪、控制UI、原投屏不变。候选pending。

## 2026-10-03 / pending / Green edge confirmed and seek-sync deployed

用户已确认绿条消失。有效区域修正944bfe0运行input360x640、tile_offset0x000f0008，解码pitch384保留，未新增CMA帧；中心点按切换源码a7aab1f及优先请求/明确回退也已部署在fb11360c UI。
同步候选源码5d0dea2/ff6109f/64633a7；专用播放器SHA256 4c38dfd89965a70bc3199e99c8eda7d592654b55254ad5494f28675312287cd5，原SDK构建rc0、SSH哈希一致。当前17570启动：first audio packet PTS0，first display PTS120000us；音频等待首帧并抛弃其之前PCM，随后持续5秒125帧/errors0，绿条window保持。尚未取得用户新的拖动同步结果，不宣称根因已完全证实或同步已验收。
旧visible播放器b69c7098保留可回退，原投屏播放器/驱动/内核未变。等待真实跳转后的首音频/视频PTS及用户体感；三冷0/3，整体pending。无新增临时脚本。

## 2026-10-03 / Planned / Seek preroll and first-frame audio gate

用户确认拖动后画面比声音滞后。旧逻辑SeekClosest后视频从关键帧直接显示，音频独立输出，未对齐首帧时间。仅专用适配器增加首个视频PTS/音频包PTS日志；目标之前的视频仍解码但不送显示，首帧成功提交前音频PCM等待；音频早于首视频PTS的整块PCM不输出，确保不提前播旧时间声音。保持绿条有效窗口、控制UI、默认声卡和投屏不变。真实同步必须用户重拖确认，未将推测称已证实根因。

## 2026-10-03 / Planned / Visible-pixel window for rotated MB32 frames

首帧实测buffer384x640/stride384，有效边界L8/T0/R368/B640。旧view把填充列包含进去。应用专用适配改用已有内核center-crop命令保留完整pitch384、输入有效360x640；该接口只支持32字节对齐起点，因此应用通过受限FE映射补齐tile内X0=8及X1=15，两路Y/UV一致，随后置寄存器ready。仅确认MB32 YUV420格式且有效起点<32、top0时启用，否则报错拒绝，不泛化到未知布局。只触及FE的0x30/0x34/0x04，不修改kernel/DTB/原投屏程序，不新增全帧复制或CMA缓冲。退出关闭映射，恢复fbdev会重设FE；实机绿条结果待验证。输出窗口仍沿用当前显示设置，此项仅移除填充，不宣称改变整体缩放策略。

## 2026-10-03 / Planned / Center toggle, explicit fallback and green-edge diagnosis

用户要求中心再次点击隐藏UI；优先800x480目标，不可用回退640x360；照片显示视频边缘绿条。现有日志旋转有效画面360x640，但DRM输入384x640，疑似对齐填充被缩放，暂不认定屏参问题。
修改仅wiliwili：中心点按切换可见；请求qn32渐进MP4并检查quality，不满足时明确请求qn16；暂不冒充已经支持DASH双轨或精确800x480。专用播放器记录首帧VideoPicture的裁剪边界，确认真实有效区域后再用既有裁剪接口处理，不改kernel/原投屏播放器。

## 2026-10-03 / pending / First-frame-gated clock deployed

源码a9a5dd3/独立入口ca71aa7，UI37f3297b8eb2ce1cb85b70790bfd5edbeee275988f417436cf5c380d5eabb810、播放器13e327372402f0f2f6a5d7f2bd9fef13958f21de78b4cb8da39773dbf37ad903，原SDK增量构建rc0/无新警告，SSH校验一致。原controls文件保留，原投屏未改。
实机播放器日志PLAYER ready event: first YUV commit succeeded，UI随后PLAYER ready: first video frame committed；当前player14715持续提交，total413/window126/errors0。软件计时在ready到达前反复保持起始tick，选定offset不变，首次实际YUV提交后走时。真实拖动加载阶段是否完全符合体验待用户验证。没有新增临时脚本，候选pending/三冷0/3。

## 2026-10-03 / Planned / Hold seek clock until first video frame

用户确认音量和拖动可用，但松开后黑屏加载时UI先走时。改为专用播放器第一次成功提交实际YUV帧时发送ready事件6，UI在该事件前保持选定时间/滑块不走时，收到后启动软件计时。仅适配器/UI修改，不改变原投屏播放器。事件表示成功提交显示硬件，不冒充精确扫描输出PTS；seek仍有网络加载等待。

## 2026-10-03 / pending / Controls deployment running

源码b86ab54/独立入口036379a，UI36956249c13c2e3833030b64ae066ca7ddf0b8054b3c218544224415f758c0ba、播放器6db4275ffcde0a9511a38e96efe08e8df85f7d0b6a822441012fde5053bddc8b，主机与板端SHA256一致。原进程9658及其子任务退出，当前UI11892/player11938，原480版本文件保留，不覆盖投屏。
启动日志PLAYER volume=41，default 48kHz stereo输出，连续5秒窗口125帧、errors0；MemAvailable24516KiB/CmaFree9780KiB。这确认命令送达和启动播放，不代替人耳音量验证/真实拖动验收。按下暂停、松开一次重建会话并按时间seek功能等待用户实测，可能有网络重缓冲等待。修改已Git跟踪，候选pending，三冷0/3。未创建临时脚本。

## 2026-10-03 / Planned / Deploy volume and drag-seek candidate

用户明确允许部署，反馈当前播放恢复正常。源码b86ab54，原工具链构建rc0；新UI36956249、新播放器6db4275f，使用独立controls文件名及启动入口，当前480文件40686916/c8f496bf保留可回退。只停止本轮记录PID9658及其自有子进程，不停止蓝牙/Wi-Fi或修改原投屏链路。新运行日志检查初始音量真实应用、解码连续性和资源占用；真实拖动/声音需用户验证。

## 2026-10-03 / Planned / Real volume and release-to-seek

用户要求音量有效、按下拖动进度即暂停、松开按选定位置继续播放。原后端明确返回未实现，不能把可拖动UI当成功能。
仅修改wiliwili专用适配：音量经已有有界Unix数据报设置0..100%，AAC PCM写ALSA前定点衰减；跳转前回收旧解码子进程/队列，新会话第一次Prefetch前执行CdxParserSeekTo目标时间，按原解析器最近可解码位置恢复，不改原投屏源码。拖动期间UI软件进度不覆盖手指位置，松开才一次跳转；实际网络重缓冲延迟如实记录。
保护原播放器/内核/驱动/默认声卡服务。测试0%静音/100%原幅度、暂停/拖动/松开、解析器seek返回值、实际用户画面声音、资源释放；候选仍pending。

## 2026-10-03 / pending / Actual loop-reset verified

新专用源码34a75f7，UI黑底源码c64596d；运行UI SHA256 406869169d80673cb498ace27a0a5cfa97c1bb5db8257aa278213f02a340fc61、播放器c8f496bf2a12828fa22dc85f6080505d5b1238f27a6c466bc3a37674c1ace7df，SSH部署一致。原SDK make -C board_linux -j2 net-player rc0。
2026-10-03 10:24:23 UTC自然播放完整MV后，播放器日志出现PLAYER loop-to-zero: seek_rc=0 UI_event_bytes=8；UI日志随即PLAYER rewind: time and progress reset。不靠视频duration或文件seek猜测，实际解析器时间回绕事件已传达。回绕后帧仍持续，最新total6310/window126/errors0。旧字节回绕判断候选已否定并删除。
本轮同时部署去掉播放页放大缩略图的纯黑背景。首页与详情封面保留。未修改原投屏播放器、内核、驱动或声卡服务。仅一次同启动自然循环验证，不代替三冷验收，整体候选仍pending。软件时间不是解码PTS精确进度，后续还应接实际播放时钟。无新增临时脚本。

## 2026-10-03 / rejected then planned / Correct loop event location

用户再次确认进度未归零；完整自然循环监测无rewind日志。字节seek=0不等于MP4时间seek=0，上一检测候选被否定。去掉HTTP字节回绕推断，新增应用专用loop_mediaplayer.c，包含原完整mediaplayer.c并仅拦截它调用CdxParserSeekTo(parser,0)的返回结果；成功后发送事件4并记录真实发送字节数。原cedar-player工作树源文件不修改，原投屏二进制不变。同时保留上一请求的黑底播放页，待实机自然循环验证。

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
