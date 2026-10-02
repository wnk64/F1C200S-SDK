# 投屏启动前 CMA 预检候选

原panel仓库candidate/cma-start-preflight-20261002，源码deda9d6，菜单集成
17f3745/d6c08a2，执行权限修正98f6254。没有新建Git工作树。

## 现象与范围

本启动9868a347、内核5.7.1 #250/CMA20MiB。原播放器在5625秒申请9MiB时
返回EBUSY，CMA位图仍有4282/5120页未被专用分配占用，但7个候选连续区间
有普通忙页不能清出。同启动前两次9MiB曾成功并释放；不是9MiB容量配置本身
错误。失败时具体忙页持有者未回溯定位，不能直接归咎于某一菜单字体。

## 候选改动

prepare-cma.sh确认旧player/sink/watch和GUI均不在运行后，执行sync，
drop_caches=1仅回收闲置文件缓存，记录前后数据，再启动原播放器。
不释放slab、不强制紧缩、不改CMA大小、内核、播放器、AIC、BS、蓝牙或音频策略。
内核现成迁移和紧缩已启用。新调用包装/应用表/小管理脚本独立存放；原入口保留。

这提高整池分配可靠性、减少分散分配，并非减少物理RAM总需求。
池仍预留9MiB，实际峰值约8.05MiB；未用池空间也属于已预留内存。

## 运行

```sh
sh /root/aic_miracast/candidates/cma_start_preflight_20261002/menu-supervisor.sh
```

先退出其他菜单或投屏管理器，禁止同时运行。新管理器复用已有网络GUI
db2d1b55，触摸选择投屏会先退出GUI/wait，再运行新预检启动器。
投屏结束回收全部自有流进程后恢复网络菜单。未添加开机服务。

--test-cast-once仅供回归：显示真实GUI2秒，正常终止并回收，然后一次性选择
第一个投屏入口，结束后仍恢复普通菜单。不是持续自动连接或生产默认模式。
部署必须chmod+x四个sh，核对应用表和脚本哈希，检查新player PID不同且存活。

## 实测

同启动四次实际9MiB申请均成功，ADB重连并保持手机原横屏循环媒体。

| 实际成功轮次 | 预检前CmaFree KiB | 预检后CmaFree KiB | 池 |
| --- | ---: | ---: | --- |
| 1 | 8692 | 15644 | 9437184字节 |
| 2：先读完整字库制造缓存压力 | 312 | 16344 | 9437184字节 |
| 3：受管理入口，修正执行权限后 | 9120 | 16084 | 9437184字节 |
| 4：当前受管理投屏 | 9448 | 16224 | 9437184字节 |

前三轮正常手机断开后都记录33子分配、peak8437760、fallback0、live0，
池全部释放；第三轮确实自动恢复菜单23464，无显示持有者重叠。
中间一次菜单集成因脚本644而exec失败/ADB超时，没有新池分配，已记录并修正；
不能将旧日志reserved行计作成功。上表是四次有效分配，不是连续冷启动验收。

当前管理23663/player23857/watch23858/sink23976，手机ADB connected=true。
实测提交2566，最近约246帧/5秒，errors0、socket丢包0、FIFO full0；
MemAvailable18820KiB约18.38MiB，CmaFree5092KiB约4.97MiB。
实际音频默认headphone，未验证本轮蓝牙有声，不与旧蓝牙CPU窗口做严格A/B。
物理画面/长时/三次冷启动0/3仍待验收，不推稳定标签。

## 回退

先手机断开并确认player/sink/watch退出，再停止本候选管理进程；
运行旧/root/aic_miracast/candidates/lvgl_connections_20261002/menu-supervisor.sh。
旧应用表仍调用lvgl_menu_20261002/cast-app.sh（408bfac2）和原默认声卡启动器
6be8390d，没有缓存预检。不得在投屏中途创建第二菜单抢显示。

## 制品

- prepare-cma.sh：f3333ad402b6cb1cc482c2fb14e52137
- start-cast-default-audio.sh：fe515f1c83402bec6ef7f0d66e63c8c3
- cast-app.sh：408bfac290de785a3458c56106457f08（相同内容，SELF派生目标不同）
- apps.conf：ccd30cdf93281bc9a562e1499a66d5fa
- menu-supervisor.sh：c4a414cf22d2d867b18dda5603891ef9
- player：a683cf70a95ba2759b5c9d2b1e0dc016
- sink：338c87bf1517807e782af48c1157904e

本地主机保存before-player/round1/round2/round3日志，不在板端/tmp归档或缓存视频。
