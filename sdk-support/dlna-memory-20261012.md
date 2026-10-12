# DLNA 大索引视频故障调查

## 计划与基线

SDK：`/home/wnk/Desktop/F1c200S_SOFT`，2767bff。板端5.7.1；gmediarender SHA256 30af359d05cfd320dc2711be2ef222730b6182c73f1c3d5ed6582546c5759871；MPlayer SHA256 18ee68533ddd4e3354063438580ad4ee81bfdd530d16d881fb5bcf3d1774d61f，来自保留的完整portable-sdk源码与构建报告，不替换播放器二进制。

输入为用户手机DLNA发送的Bilibili MP4：480x360、H.264 High/yuv420p/level51、AAC LC；文件67576562字节，moov2244559字节，视频样本约54000、音频样本约77000。只在电脑读取前3MiB验证结构，未将完整视频缓存到板子，URL令牌不入Git。现代主机解析无错误，板端旧自动解析报Invalid UE golomb/Overread VUI。

内核在打开媒体阶段OOM杀主播放器，普通内存不足；当时空闲21860KiB中CMA17252KiB，普通空闲4608KiB，内核min4816KiB。主播放器死亡后缓存进程PPid=1长期遗留。当前Slab约12.4MiB，其中kmalloc-4k约4.34MiB。原播放器mov/nocache/faad可以进入解码，但限时测试未完成；mov/cache256/faad仍OOM，不能标为修复。wiliwili后端HTTPS+明确CA能显示90帧后音频时钟停滞，不能据此替换DLNA后端。

临时实验假设：结束投屏后仍驻留的AIC驱动、PBC与DHCP属于DLNA闲置资源；释放这些资源可能让原播放链保留足够普通内存。先核实旧sink/player/rtsp PID均不存在，再仅停wlan1对应进程、卸载已安装aic8800_fdrv，保留BS联网/SSH/H5蓝牙、Cedar/DRM/普通网络。测试后使用原load-aic/start-cast脚本恢复，不修改原模块、内核或投屏源脚本。记录前后普通内存与Slab；依赖回归必须验证AIC/wlan1再启动。不能把软件重启当三次冷启动。

拟修复：仅DLNA回收逻辑以独立进程组终止整个播放器家族，检测主进程异常时清理缓存进程，再恢复等待界面。是否加入资源释放或解析选项取决于上述对照结果，不做推测性内核修改。

## 对照结果与候选计划

1eb868b已记录计划。停wlan1对应闲置后台并卸载现有aic8800_fdrv后，MemFree21880→26276KiB（增加4396KiB），Slab12740→9332KiB（减少3408KiB），CMA不变16720KiB；BS路由器ping/SSH仍正常。原MPlayer默认解析、512KiB网络缓存、faad、12MiB集中池，用户同一条URI成功显示180帧，末尾A/V约6.0秒、误差约0，退出0；集中池峰5484544、fallback0、live0。证明闲置AIC普通内存是该输入启动失败的关键条件，而不只是视频大小或网络连接问题。

仅SDK候选：runtime-lan/dlna-app.sh与release-idle-cast.sh在DLNA入口核实无活跃cast后，结束经过cmdline核验的wlan1辅助进程树并卸载原AIC驱动；普通BS/SSH/BT不动，AIC由原投屏加载器在下次投屏时恢复。runtime-lan/output_cedar.c从精确锁定原模块cc208b25...派生，只增加512KiB缓存与独立播放器session/进程组；停止、切URI、主播放器被OOM杀后清理整个缓存进程组，不让新播放与旧缓存同时占内存。sdk.py严格校验已知源hash才应用此模块，原组件源码和所有播放器/内核二进制不改。

待验证：host孤儿缓存进程组回收测试、生成器与shell语法测试、板端同一手机视频持续播放/暂停/定位/断开/再次连接，以及原AIC投屏入口重新注册wlan1。三冷启动0，不标稳定发布。部署前保留板端原gmediarender/dlna-app.sh的校验备份；新增helper可删除回滚，原MPlayer哈希不变。

01ac414已提交候选，72项测试通过，包含实际fork/setsid/模拟主进程SIGKILL/缓存后代回收的host测试。LAN构建退出0，gmediarender38e4019287a091581ce5e0ba6cb6eba166fbfdf250096eb2eae6550c4ab91afe；launcher bcf2acd8...；helper5b642023...。原source、播放器、kernel未改。SDK原sdk.py因Windows传输模式变化误设为可执行，后续恢复原644。

恢复AIC首次失败：原cast-app.sh仅凭/tmp会话目录存在选session，但卸载后wlan1不存在，报告existing GO5745 unavailable。只在SDK生成入口的精确替换中增加接口存在条件，接口缺失选择cold；原源脚本/播放器/驱动不修改。需验证再初始化+三击退出、再次进入DLNA释放闲置AIC；不在失败状态上宣称回归通过。

100abec生成入口已修正，73项测试通过。用户明确允许回滚备份和部署；板端原renderer/dlna-app/cast-app分别保存在原路径加.pre-dlna-oom，hash30af359d.../0e71e20b.../beada998...，候选renderer38e40192...、cast入口2ef1781e...核验后部署，MPlayer18ee6853...未变。再次恢复时原cold流程正确拒绝覆盖旧/tmp会话目录；补充helper在确认无活跃cast、停止已核验后代后，只清理枚举的生成会话文件/空目录和已确认FIFO，未知文件保留并报错，拒绝symlink。不改原cold安全保护。需要重新验证释放→cold恢复→再次DLNA全过程。

## 实机回归

956159d生成/部署helper，SHA25697d88714...。只移除了失效/tmp/manual_miracast_20260920内的已知生成文件、空目录及旧FIFO；原源码、固件、回滚文件保留。投屏入口重新加载AIC并注册wlan1、GO5745就绪、模拟三击正常退出，退出0。随后新DLNA入口成功再次释放闲置AIC，没有破坏BS联网/SSH。

用原手机同一Bilibili长视频直接网络播放（不是下载到板子）：进度3→8秒，暂停固定9秒；定位30秒后进度36→39秒；主播放器12877模拟SIGKILL后，缓存12879一并消失，UPnP状态STOPPED；重新播放进度3秒；停止后无任何MPlayer遗留。原解析器的Invalid UE golomb/VUI警告仍存在，但此输入实际解码推进、音视频控制通过；没有声称解析器源码已修复或警告消除。需要用户实看画面/听感与三次物理冷启动，冷启动计数仍0。

进一步重复测试在重播前重新SetAVTransportURI清零位置，避免沿用旧位置而误判重播成功。helper增加/proc存在检查，只消除过期PID诊断噪声，不改回收范围。最终镜像待重新生成；保留独立初始镜像与全部Git源。不把上述单次/软件层回归当稳定发布。

c632782两次完整视频回归通过；第二轮重播明确从新URI0秒推进2秒，暂停8秒固定，定位后30→38秒，模拟OOM主13034/cache13037均消失。菜单恢复、BS ping/SSH正常，MemAvailable35016KiB，Slab9324KiB。初步镜像e3a813e4...已生成。最后检查发现cast DHCP旧wrapper12436/server12449仍存活（PID文件已失效），补充只匹配精确wlan1 DHCP wrapper argv的扫描、对核实PID-starttime的后代增加TERM后有限KILL，并把已知udhcpd.pid/leases加入临时状态清理。未知进程/文件仍不动，不扫描终止普通BS客户端。需再次检查无残留并更新镜像。

最后清理同时覆盖完全孤立的/usr/sbin/udhcpd -f /opt/f1/apps/cast/udhcpd.conf服务器（早期实验遗留PID377），只匹配这个cast专用配置，不匹配普通网络udhcpc。避免旧服务器继续占UDP67使下一轮投屏DHCP出错。既有wrapper树已清理，恢复投屏GO并再次释放返回0；最终继续核验专用DHCP/wpa/PBC和所有MPlayer无残留。

最终候选代码248b62f，73项单测通过，rootfs-integrated及sd-image成功生成。最终helper SHA25681036181db011948495e82a4ac5e966848b517b033b6856fa5cc543296c53d06，经SCP/hash核验部署。末尾ps确认无AIC后台、cast DHCP/PBC/wpa、DLNA或MPlayer遗留，仅BS/SSH/BT/菜单保留；MemAvailable35332KiB，Slab9296KiB，CmaFree16756KiB。已恢复菜单、保留三个.pre-dlna-oom回滚文件；临时测试脚本与传输副本清理。真实视听确认和三次物理冷启动仍待用户验收。
