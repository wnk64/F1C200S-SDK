# wiliwili 蓝牙音频故障恢复候选

原源码LVGL/wiliwili_lvgl，原分支candidate/f1c200s-fbdev-20261002，基线f7422a3，原播放器37785e3252a4396d6e3bd1202d9d683a5e79455650bcc0dde9fcdd75c38bb911。保护原cedar源码、Miracast播放器、蓝牙驱动和rootfs音频路由；不新建工作树。

实测：BlueALSA BT socket timeout后播放器PCM Broken pipe/No such device，音频线程退出而audio_clock_active未清除，视频等待停止更新的音频时钟。播放器还处于SIGSTOP全线程停止，OSD不可处理显示命令；GUI仍运行。H5校验和乱序错误与重连接同时出现，但日志不足以确定驱动根因，不能因这轮失败否认之前稳定投屏。

计划：对照投屏恢复策略，仅改应用wrapper。非阻塞PCM写、最长500ms等待、失败回退耳机并三秒后重试默认设备；检查audio-route.state切换，重新打开输出。音频线程不因失效PCM直接退出；audio_clock超过750ms不更新则视频解除同步等待。暂停使用现有OSD socket命令8/9协作暂停解码/音频/预读，不冻结OSD与退出线程；暂停释放PCM，恢复重开，不保留断开的蓝牙句柄。停止使用drop代替无界drain，仍保留主机两秒停止超时回收。

待构建与实机自动测试：暂停/控制显示/恢复、跨循环、蓝牙断连重连及运行中切换、退出回菜单。每个测试探针显式开启，不带入正式启动环境；三次冷启动及完整回归前不称稳定。保留旧loop二进制和入口，单独部署候选后再切入口。

源码745d5a9、测试c2a71d4，原目录增量make -C board_linux -j2 all net-player selftest rc0。候选应用8815dc7ad7d002bcb5abf62817827ad359cf85a62476a42f19e61abdbfaf1cb7、播放器2225926fcdd10b7d2e2182d1a532d60f2906a8dac1e3958223afe1268c642c81，分别独立部署wiliwili-audio-recovery、cedar-audio-recovery，正式launch-loop入口未切换，旧版本保留。

第一轮旧完整selftest失败于seek195秒：这次流首关键帧190秒，preroll未完成，无法证明音频恢复问题已解决。随后显式F1_AUDIO_RECOVERY_TEST测试跳过无关seek：实际帧166ms→暂停位置不变→恢复1066ms→20秒观察19066ms，rc0，播放器退出rc0。测试程序末尾固定文字仍称two seek-loop cycles，实际本轮跳过该部分，不能以其文字当作循环通过。用户随后报告蓝牙仍异常，并要求冷重启；停止自动测试，正常停止菜单/sync/poweroff，通过COM3明确OFF/ON间隔3秒执行。蓝牙根因未定位、长时播放/重连未验收、正式入口不部署本候选。一次性执行包装删除。

## 重启后先连接蓝牙的复现 / rejected部署

用户报告声音正常、画面及进度卡住。正式入口核验仍是旧loop播放器37785e3，应用仍wiliwili-loop-lvgl，本候选未接入正式菜单。随后自动候选测试两轮均从200ms到恢复1066ms，20秒进度观察超时，测试失败后退出回菜单，不部署候选。BlueALSA日志此次无断链错误，不能归咎蓝牙。

关键现场：第二轮候选PID2208，显示线程2211阻塞drm_wait_vblank_ioctl；decoder持续VDECODE_RESULT_NO_FRAME_BUFFER（统计results[4]增长），网络继续预读。LCD IRQ26计数连续间隔1秒均12692，无增长；TCON GINT0读取0x00008000（vblank状态位存在但使能位31关闭），GCTL0x80000000、TCON0_CTL0x808001E0，DRM crtc active=1。明确显示刷新中断等待阻塞帧回收，音频仍可运行，因此进度（实际提交帧PTS）也不增长。是谁关闭使能仍未定位，不改寄存器、不关闭vsync、不推测硬件。下一步查DRM vblank使能/引用生命周期及OSD并发提交，必要时加可关闭内核探针前先执行kernel baseline guard；原kernel保持不动。

## 最新结论 / 最小帧预留修正

上段“显示中断等待导致卡死”的结论不成立，后续OSD隔离fe569f5仍卡1.066秒，LCD IRQ正常增长、GINT0=0xC8000000。idle显示没有提交帧后中断关闭可为DRM正常节能，不能当成根因。新增探针ef250d5/fd026e9：valid0/display_queue0/free_queue0，借帧29还帧28（只保留正常扫描帧1），audio_pts继续增长而video_pts停1066625。没有发现借还泄漏，问题在解码FBM预留。

当前保护播放器config.h的BUF_CNT_4_LIST/ROTATE/SMOOTH均0，H264参考DPB占满且硬件旋转/扫描输出还需帧。0622d18仅在wiliwili InitializeVideoDecoder wrapper配置display预留1、rotate预留1，不改保护源码或CMA总量。对应文件本次总共四个探针/最小修正，全部默认debug关闭或显式诊断开启。

同一蓝牙先连接、同一在线视频、同一测试：OSD隔离帧166→暂停冻结→恢复1066→19100，rc0；完整OSD帧166→暂停冻结→恢复1133→19133，rc0。两轮均蓝牙default输出正常，借还差2为显示排队/扫描持有，video_pts持续增长，audio/video差几十毫秒；退出正常，返回菜单。MemAvailable24988KiB/CmaFree7212KiB，没有增加整屏UI缓存。新播放器SHA256ba2a8b87c119a361072d1ec3f6636a45cecf44a6abdc6be34a0a8b00d2a92b0f。声音是否耳听、长时/循环/seek195/冷启动完整依赖尚未验证，不称稳定。

部署计划：新增launch-audio-recovery-with-menu.sh，仅正式菜单wiliwili入口切至新应用8815dc7和上述播放器，默认F1_PLAYER_DEBUG=0；无OSD禁用变量、不自动selftest，不修改投屏入口。原loop应用、播放器、启动器完整保留。回退apps.conf最后一行至launch-loop-with-menu.sh。修正自动测试结尾文字，audio测试不再错误声称two seek-loop cycles。

部署已执行：应用源码cb2fc19，根入口eb55d01，板端最新apps.conf SHA2561e28b728936a047627d7867e5008093325b05a76ab213bd47bd3ed4143c438cb，应用与播放器哈希一致，菜单正常restart。一次性探针包装删除。正式入口保留完整OSD，未改原投屏。新候选只有针对性短测试通过，长时及完整冷启动仍pending。
