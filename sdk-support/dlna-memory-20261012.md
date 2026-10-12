# DLNA 大索引视频故障调查

## 计划与基线

SDK：`/home/wnk/Desktop/F1c200S_SOFT`，2767bff。板端5.7.1；gmediarender SHA256 30af359d05cfd320dc2711be2ef222730b6182c73f1c3d5ed6582546c5759871；MPlayer SHA256 18ee68533ddd4e3354063438580ad4ee81bfdd530d16d881fb5bcf3d1774d61f，来自保留的完整portable-sdk源码与构建报告，不替换播放器二进制。

输入为用户手机DLNA发送的Bilibili MP4：480x360、H.264 High/yuv420p/level51、AAC LC；文件67576562字节，moov2244559字节，视频样本约54000、音频样本约77000。只在电脑读取前3MiB验证结构，未将完整视频缓存到板子，URL令牌不入Git。现代主机解析无错误，板端旧自动解析报Invalid UE golomb/Overread VUI。

内核在打开媒体阶段OOM杀主播放器，普通内存不足；当时空闲21860KiB中CMA17252KiB，普通空闲4608KiB，内核min4816KiB。主播放器死亡后缓存进程PPid=1长期遗留。当前Slab约12.4MiB，其中kmalloc-4k约4.34MiB。原播放器mov/nocache/faad可以进入解码，但限时测试未完成；mov/cache256/faad仍OOM，不能标为修复。wiliwili后端HTTPS+明确CA能显示90帧后音频时钟停滞，不能据此替换DLNA后端。

临时实验假设：结束投屏后仍驻留的AIC驱动、PBC与DHCP属于DLNA闲置资源；释放这些资源可能让原播放链保留足够普通内存。先核实旧sink/player/rtsp PID均不存在，再仅停wlan1对应进程、卸载已安装aic8800_fdrv，保留BS联网/SSH/H5蓝牙、Cedar/DRM/普通网络。测试后使用原load-aic/start-cast脚本恢复，不修改原模块、内核或投屏源脚本。记录前后普通内存与Slab；依赖回归必须验证AIC/wlan1再启动。不能把软件重启当三次冷启动。

拟修复：仅DLNA回收逻辑以独立进程组终止整个播放器家族，检测主进程异常时清理缓存进程，再恢复等待界面。是否加入资源释放或解析选项取决于上述对照结果，不做推测性内核修改。
