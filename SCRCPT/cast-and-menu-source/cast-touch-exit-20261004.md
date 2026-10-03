# 投屏等待阶段中心三击退出

原根仓库原分支，不新worktree。仅新增cast-wait-touch.c、Makefile独立目标，修改cast-app.sh生命周期与菜单cast入口；不改kernel/驱动/投屏协议/播放器。板端新增同目录cast-wait-touch与cast-app-touch.sh，旧cast-app.sh保留可回退。

触摸沿用已确认FT6236 I2C0/0x38及raw→portrait方向，不加载LVGL或新显示层。原生480x800中心区域x140..339/y260..539，两秒内三次按下释放（40..500ms），移动超过30px/区域外/多触摸/读失败/长按重置。不以轮询次数当点击。使用同sink.pid且exe必须匹配实际sink作为连接保护；检测sink活着即停止退出检测并释放触摸进程。shell收到退出码10后再次检查sink，关闭自己启动的watch/player/sink并正常返回菜单，不动独立投屏或Wi-Fi/蓝牙。

探针--selftest为纯计数测试，--simulate-triple只有显式F1_CAST_EXIT_PROBE=1的cast启动才使用，用于自动测试cleanup与退出；正式无此变量走真实触摸。计划主机/ARM计数测试，未连接实际投屏初始化后模拟手势返回、检查无遗留播放/触摸进程及SSH/菜单回归。真实三次触摸和已连接时不会退出需实测，冷启动完整验收pending。
