# 先连接音响再投屏：2026-09-30

用户要求按顺序冷重启、连接已配对音响、确认默认声卡，再启动投屏；不改驱动、源码、内存参数或开机服务。

上一启动16bebc81在播放中出现内核OOM：约505秒sink写FIFO触发内存分配失败，507秒内核杀BlueALSA、510秒杀player-pool，不能把该失败记录为稳定共存。代理没有主动kill这些进程。BlueALSA后由既有守护进程恢复，音响已断开、GetPCMs为空，路由回耳机。连接音响约405秒还出现H5校验/乱序警告；不能单凭时序证明OOM由蓝牙导致。当前raw H264投屏没有音频流，默认声卡仅对新打开的音频播放有效。

## 本轮顺序与实测

核对COM12电源控制、COM13板子，sync并确认没有启动FAT或debugfs挂载。调用既有拓展坞控制工具、临时指定COM12，收到OK CH2 OFF/ON，断电间隔3秒。COM13重枚举后采集启动日志，本轮启动ID06a569e2-02b6-4456-af16-590974b1ce88，内核5.7.1 #248，BS SSH10.0.0.234恢复。
通过BlueZ Device1.Connect连接已配对Wt-070(12:11:71:41:9C:4A)，方法成功；路由自动变为bluetooth 12:11:71:41:9C:4A，运行态ALSA default选择BlueALSA A2DP。GetPCMs返回source/48000Hz/2声道/SBC；开流前MemAvailable30144KiB。连接阶段仍见H5校验、短包和乱序警告，未掩盖。
随后才加载既有校验AIC loader/rxguard，8d80→8d83→wlan1；原协议配置MD5一致，WFD命令均OK。候选player-pool1259e0f3f151a06fc2270ff885dc4443，CEDAR_CMA_POOL_MB=7、90度硬解旋转、raw640x480@60、default音频，FIFO不写视频文件。先初始化播放器等待FIFO，再创建5745MHz GO DIRECT-DH，COMPLETED/AP-ENABLED。DHCP、请求触发PBC、DHCP触发RTSP监听器已启动；播放器PID569，默认路由仍BS，音频路由仍蓝牙。等待手机连接，尚未称长播稳定或蓝牙音频投屏成功。

原内核/zImage、屏幕DTB50499cb、播放器默认路径和驱动均未覆盖。不调min_free_kbytes、不调CMA、不启用8723BU。临时控制/构建包装清理；运行态PBC/RTSP监听器保留供当前测试。软件状态回退可重启清除本次/tmp投屏，并关闭音响恢复耳机默认。
