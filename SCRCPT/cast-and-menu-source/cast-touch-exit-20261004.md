# 投屏等待阶段中心三击退出

原根仓库原分支，不新worktree。仅新增cast-wait-touch.c、Makefile独立目标，修改cast-app.sh生命周期与菜单cast入口；不改kernel/驱动/投屏协议/播放器。板端新增同目录cast-wait-touch与cast-app-touch.sh，旧cast-app.sh保留可回退。

触摸沿用已确认FT6236 I2C0/0x38及raw→portrait方向，不加载LVGL或新显示层。原生480x800中心区域x140..339/y260..539，两秒内三次按下释放（40..500ms），移动超过30px/区域外/多触摸/读失败/长按重置。不以轮询次数当点击。使用同sink.pid且exe必须匹配实际sink作为连接保护；检测sink活着即停止退出检测并释放触摸进程。shell收到退出码10后再次检查sink，关闭自己启动的watch/player/sink并正常返回菜单，不动独立投屏或Wi-Fi/蓝牙。

探针--selftest为纯计数测试，--simulate-triple只有显式F1_CAST_EXIT_PROBE=1的cast启动才使用，用于自动测试cleanup与退出；正式无此变量走真实触摸。计划主机/ARM计数测试，未连接实际投屏初始化后模拟手势返回、检查无遗留播放/触摸进程及SSH/菜单回归。真实三次触摸和已连接时不会退出需实测，冷启动完整验收pending。

源码a26d456新增功能，0505381补P2P关联保护（直接wpa控制socket STA-FIRST，不启动CLI进程，不修改协议），3942b56修正本版本supplicant无STA时返回空响应而不是FAIL。未知查询状态不触发退出，保护正在握手的手机；helper与shell收到退出后分别再次确认，减小连接竞态。第一次保护版本空响应误判导致helper停止监听，已停止该次测试并修正，不计成功。

原GCC6.4 make -C LVGL/menu cast-wait-touch，主机gcc/板端ARM --selftest通过（真正按下释放三击/超时/长按/移动/范围外重置），shell sh -n通过。实际未连接启动投屏到ready后模拟三击，最终修正版本记录triple-center exit requested与returning to menu，整体rc0，已清理watch/player/sink/触摸监视器，恢复菜单及SSH。独立检查触摸helper PSS115KiB/RSS1084KiB，其中共享clean1024KiB、private60KiB，不启动LVGL、不增加视频CMA。

59c225c切菜单cast第一项至cast-app-touch.sh，其他应用行保留。SCP核验helper0ec48d5c6f7da49ded0a585f1e92c825be2f00713a77aefd355816a49d9fe816、script31fe30b11011f8a527beb3eb511ee1a0fa8b373dc76ebe4e54f63ba85f99f097、apps001ce6f4f9134d8c2b60f23f70da5500494923691ec9ab2276dd24c0e919c99e。最终菜单supervisor2558，正式无F1_CAST_EXIT_PROBE，不自动测试。真实触摸与真实已连接不退出、完整冷启动投屏/音频回归pending，不宣称稳定归档。回退菜单cast行至旧cast-app.sh，旧脚本未覆盖。临时执行日志清理，无遗留测试进程。
