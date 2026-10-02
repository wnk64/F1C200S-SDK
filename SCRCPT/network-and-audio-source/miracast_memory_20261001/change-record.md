# 投屏前释放无用内存

## 2026-10-01 计划

用户明确要求投屏开始前尽可能释放内存。原RTL工作树基线6c4486e，内核完整基线165df40/#248、zImage2d16ba197f384e0570071aabf2094c44不变。只新增运行态准备工具并挂接既有AIC加载入口，不重编内核，不改变CMA、水位、OOM策略、解码缓冲或Wi-Fi/蓝牙设置。
先检查没有打开虚拟摄像头/测试声卡的应用，确认Dummy/Loopback卡号，然后解绑vivid.0、vimc.0、snd_dummy.0、snd_aloop.0。它们是测试设备，不属于Cedar解码、真实LCD、Codec耳机或蓝牙音响。每解绑一项记录Slab、不可回收Slab、MemFree/CmaFree及普通区空闲差值。保留蓝牙/BlueALSA/D-Bus、BS Wi-Fi/SSH、Cedar/DRM、I2C电源和RTC。
记录到/run/f1-miracast-memory/devices，支持restore重新绑定；重启也能恢复。工具为可重复投屏准备流程，保留并Git跟踪；未做稳定验收、不推GitHub。

## 重启后分步验证计划

前次prepare-result.log为空，未得到任何解绑成功证据；用户要求冷重启后已恢复原配置，启动ID1af6e545-d0cf-4256-a3d8-cbc78669683e。不能把此前超时归因于已解绑设备。修改工具检查方式：使用一次BusyBox fuser代替逐/proc文件描述符创建readlink子进程，并支持prepare单设备目标，避免低内存时大量创建子进程。先Dummy、再Loopback、再vimc、最后vivid；每步确认SSH和内存，暂不部署AIC默认入口，不重编内核、不改内存水位或蓝牙。

分步实测及restore/重复prepare通过。vimc解绑普通区空闲增加约2716KiB，vivid约3792KiB；清理后普通区空闲约8900KiB。已部署准备工具4a5184e和AIC启动前调用dec5f234，Codec/BT/SSH保留；用户连接音响并投屏后MemAvailable约20MiB、ION仍单块7MiB。完整冷启动验收未完成，不称稳定版。

## 2026-10-01 内核精简配套修正

候选内核a62a764关闭Dummy/Loopback后，真实Codec可能变为card0，旧工具固定检查C0/C1并要求其为测试卡，会误拦截投屏。改为按/sys/class/sound/card*/id识别Dummy/Loopback，只检查对应的准确设备节点；没有测试卡时跳过其检查，真实Codec不被当测试卡。仍不修改音频路由或正在播放的流。源码和准备入口均原工作树Git跟踪。
