# 2026-10-02 成功投屏源码快照

GitHub 账号已由 Wnjbk 改为 **wnk64**。当前仓库：
`git@github.com:wnk64/licheepi_nano.git`。
历史文档中的 Wnjbk 地址保留作为历史记录，不再作为新上传目标。

本快照保存用户确认可以使用的 800×480 投屏、Cedar 硬解、90度原生旋转及蓝牙音频链路。
这是成功运行快照，不覆盖旧稳定标签；连续三次完整冷启动验收尚未完成。
不改变当前板端、不新建开发工作树、不切换旧驱动。

## 完整源码入口

同一 GitHub 仓库的分支分别保存独立组件的完整 Git 历史：

| 分支 | 源码 |
| --- | --- |
| miracast-snapshot-20261002-kernel | Linux完整源码、.config、DTS、CMA20启动脚本源码 |
| miracast-snapshot-20261002-aic8800 | 实际使用的UGREEN驱动源码53bec9e，包含loader及rxguard，不含未采用msglen候选 |
| miracast-snapshot-20261002-player | 完整播放器源码5685c63，诊断/内存池/旋转/空闲退避及测试 |
| miracast-snapshot-20261002 | 完整接收器源码、测试、当前脚本、协议源码及本快照说明 |

最后一个分支内 `runtime/snapshot-20261002` 是归档入口。
该目录的 `SHA256SUMS` 与 `manifest.json` 记录精确源码提交、文件哈希及大小。
`SHA256SUMS`覆盖Ubuntu本地全包；GitHub目录使用`sha256sum -c GITHUB_SHA256SUMS`，不要求远端存在本地SDK大文件。
最终入口为快照分支miracast-snapshot-20261002及校验标签miracast-snapshot-20261002-verified。
verified仅指归档和远端哈希校验，不表示已经通过三次冷启动稳定验收。
`sources/` 额外保存源码压缩包，不能用旧播放器已跟踪的构建对象代替重新构建。
`runtime.tar` 保存白名单选出的板端实际模块、固件、协议程序、播放器、接收器及初始化脚本，不包含家庭Wi-Fi密码、SSH密钥、访问令牌或视频。

## 已核对运行版本

- Linux5.7.1 #250，zImage MD5 bffc71e8bcd28ef7f1f18616af72ab38。
- DTB MD5 50499cb0527d269e0da7e436c092f643，CMA总20MiB。
- AIC loader bb2d68ed26fbb8d3cee554af0aa7735a。
- AIC fdrv 334699cc17b7201a23c790306c64b381。
- AIC固件 01acfbebdfb15755e3fe853e7bc95c7d。
- 播放器 a683cf70a95ba2759b5c9d2b1e0dc016。
- 接收器 ff58d9fc8a47606e926575b65c6450ab。
- RTL8723BS15MHz负责普通联网/SSH；AIC只负责5745MHz投屏；8723BU不开机加载。
- H.264通过命名管道 `/tmp/aic_h264_live.fifo` 实时传输，不录制到视频文件。

## 构建

内核原树 `/home/wnk/LicheePi_Nano/linux_musb_clean_ep1_20260811`，
要求已有匹配 `.config`、`vmlinux`、`System.map`、`arch/arm/boot/zImage`。
工具链 `/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-`。
先以同ARCH/CROSS_COMPILE/LOCALVERSION参数执行kernelrelease，必须正好5.7.1。
构建 `make ARCH=arm CROSS_COMPILE=<上述前缀> LOCALVERSION= -j8 zImage`。
持久构建用tmux，禁止clean/mrproper及擅自换工具链。本次归档未构建内核。

AIC源码53bec9e的实际驱动位于 `drivers/aic8800`。
外部模块构建须指定匹配内核目录、ARCH=arm、上述Linaro前缀及LOCALVERSION=，
并沿用该源码内Makefile配置；loader源码从3a8423f至53bec9e无变化。
原构建前提包 `aic-build-prerequisites.tar` 保存旧驱动构建对应的内核产物；
不能宣称当前#250构建已重新生成这两个历史模块。部署前逐个确认5.7.1 vermagic。

接收器原树 `/home/wnk/LicheePi_Nano/third_party/lazycast_host_20260721`：
`make -f Makefile.sink -B`，使用原Buildroot GCC6.4前缀及ALSA sysroot。

播放器原树 `/home/wnk/f1c200s_display_480x800_candidate_20260914/player-src`：
`make -B -j4` 然后同工具链 `strip cedar_drm_player`。
依赖原Buildroot output/host、output/target/usr/lib及libcedarx-custom头文件。
本地完整构建依赖包另存于Ubuntu归档目录，源码包在GitHub；工具链不冒充源码。

补充源码：sources/panel.tar.gz保存实际外置屏幕驱动源码；sources/rtl-support.tar.gz
保存RTL8723BS外置H5蓝牙和自研音频路由源码；sources/bluealsa.tar.gz及sbc-1.3.tar.xz
保存完整BlueALSA/SBC来源。support-runtime.tar保存对应音频运行文件、插件及板端模块。
Buildroot个人联网配置在归档中替换为YOUR_SSID/YOUR_PASSWORD模板，原工作树不改。
SHA256SUMS包含本地全部归档项；GitHub不存374MiB SDK等大文件，使用manifest核对
Ubuntu本地完整恢复包。GitHub仍保存所有列出的源码与有界恢复产物。

## 脚本与恢复

`runtime.tar` 的旧cold-start脚本仍选择旧接收器42d0cfa8，仅用于追溯。
本快照 `start-cast.sh` 保留同成功流程，唯一更新接收器路径/哈希和批次交接开关。
配套wpa.conf/pbc-watch.sh/rtsp-watch.sh从已运行流程取得，放在脚本同目录。
此归档版本未部署到板端，不能视为已完成其cold模式验收。

播放器：CEDAR_FRAME_GAP_STATS=1、CEDAR_CMA_POOL_MB=9、CEDAR_ROTATE=90、CEDAR_AUDIODEV=default。
接收器：WFD_VIDEO_RX_BATCH=1、WFD_VIDEO_COALESCE=1、WFD_RTP_BATCH=1、WFD_LOSS_IDR=1；异步输出及音频由已运行RTSP watcher设置。
输入：`--raw-h264 800 480 60 /tmp/aic_h264_live.fifo`。

恢复必须在确认无旧会话后进行，不在正在投屏时覆盖或解压到系统根目录。
恢复runtime.tar须先查看成员，按白名单选择文件，经SSH传输、哈希核对再部署。
boot.scr仅部署到FAT分区文件，不raw-write分区。24MiB回退文件保存在归档内原rollback位置。

## 实测边界

详见 `RESULTS.md` 和 `投屏优化记录.txt`。CPU最新六窗口70.32～77.13%，可用RAM约18.4MiB，CMA空闲约7.7MiB，接收丢包0。
ON/OFF/ON各样本无超过250毫秒的显示提交间隔，但内容阶段不同，仍有微抖动及音频欠载。
显示提交次数不是物理屏幕刷新率，端到端延迟未精确测量。
