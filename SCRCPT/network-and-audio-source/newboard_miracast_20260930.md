# 新板投屏测试记录（2026-09-30）

状态：计划启动，待手机连接和动态画面验证。只恢复既有成功手动流程，不修改驱动、内核、播放器或开机联网配置。

基线：Linux5.7.1 #248，15MHz单线8723BS管理网络rtlwlan0/10.0.0.234，启动ID b1b1d3d9-9a4b-475a-b434-3708d726e69c。AIC初始8d80且模块未加载。没有桌面或本地播放器；MemAvailable约30148KiB。

固定组件：loader bb2d68ed26fbb8d3cee554af0aa7735a，隔离固件01acfbebdfb15755e3fe853e7bc95c7d，rxguard fdrv334699cc17b7201a23c790306c64b381。两模块版本必须5.7.1。ARM sink lowest ceeeb9958f8f84f2a0b7c6c3df63db0e，dtsview播放器11386abeb2137bfb3f28c47832b05552。

按步骤注册wlan1，启动既有WPA/WFD协议，5745MHz GO，192.168.49.1 DHCP。收到手机PBC请求时自动开启PBC，DHCP发现手机地址后启动ARM sink。FIFO /tmp/aic_h264_live.fifo，H.264不写磁盘，90度硬解旋转，raw640x480@60，原生480x640居中、不缩放。音频使用default，不沿用旧板hw:2,0索引。

运行日志及监听器在/tmp/manual_miracast_20260920，沿用原成功监听器，不调用旧驱动/旧一键启动脚本。保持BS WPA/DHCP/SSH及默认路由不变。保护AIC/MUSB、Cedar/CMA/DRM、RTC/I2C、蓝牙与声卡路由；启动存在不代表动态画面验证通过。

回退：停止本次投屏进程并撤销wlan1 GO，不改启动FAT或BS配置；重启可清除/tmp会话，AIC回到按需未加载状态。不推GitHub，不做稳定验收声明。监听器为当前手机测试所需，测试结束再清理。

## 本轮启动结果：等待手机

既有load-aic-for-miracast工具按校验哈希加载后，8d80→8d83→wlan1成功。板端没有modinfo命令，改用strings核对两个模块的vermagic，均为5.7.1 mod_unload ARMv5 p2v8。
WPA/WFD配置逐条执行，子元素0/1/6/11本次全部OK。先初始化原生旋转播放器等待FIFO，再停止搜索并建立5745MHz GO，返回COMPLETED/AP-ENABLED，SSID DIRECT-VJ。没有调用历史一键启动脚本。
DHCP、请求触发PBC和DHCP触发RTSP监听器运行中；目前尚无手机DHCP请求，sink未启动，不宣称投屏成功。播放器日志为90度native480x640，raw640x480@60。播放器PID1719，WPA1636，DHCP1787，PBC监听1790，RTSP监听1791。
启动后同一boot_id、SSH正常，默认路由仍rtlwlan0，增加的只有wlan1直连网段。MemAvailable20464KiB（开流前），不能代替解码峰值。保留运行态监听器供本次手机连接；脚本均为既有原样副本，传输MD5已与本机核对：wpa.conf300e8b756b24d8e3e9e0288e7b8e4f54、pbc-watch32a3fa3d7f7d100a38364299cda62e08、rtsp-watch905e81e9545d5b20b6a18f2081161696。

## 手机已连接：协议与首帧证据

手机Redmi K60发出PBC请求，监听器回复OK，DHCP分配192.168.49.52，ARM sink PID2234启动。RTSP SETUP/PLAY均200 OK，Negotiation successful；首批RTP后请求IDR。播放器日志确认set yuv view src=480x640 out=480x640 coord=0,80，内核也记录显示层配置。该证据说明协议和显示提交已进行，不代替用户确认持续动态画面。
首次开流后wlan1 rx_bytes9676854，MemAvailable14480KiB，启动ID不变且SSH正常。用户视觉确认及长播、重连均待验证。
随后10秒rx_bytes28851656→33153294，约增加4.30MB，RTSP保活回复200 OK，MemAvailable14484KiB。播放器日志没有新增解码错误；系统UDP RcvbufErrors累计12，不能称零丢包，也不能将流量增长当动态显示验收。
