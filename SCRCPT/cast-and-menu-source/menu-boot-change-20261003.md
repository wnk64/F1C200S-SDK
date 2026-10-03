# 2026-10-03 菜单开机显示修复

## 计划 / pending

源码：/home/wnk/Desktop/F1c200S_SOFT，原分支 candidate/cma-start-preflight-20261002，基线 57ad956。原菜单二进制 SHA256 c3d363346434285edfef9285d8611059742030a9124ad3223f7cab8e93fe5ba4。保留原候选目录，不新建工作树、不改内核、驱动、播放器。

实测：COM4 115200，菜单 supervisor PID255、GUI PID263 从启动时正常运行；DRM 原生480x800输出 active=1。SSH恢复后读取 framebuffer，画面主要是内核日志、残留少量菜单标题，证实 fbcon 覆盖 GUI。platform.c 仅隐藏光标，没有 KD_GRAPHICS。修改为打开 tty0，保存 KDGETMODE，申请 KD_GRAPHICS，退出恢复原模式；失败明确输出错误。内核及串口日志不关闭。

文件：LVGL/menu/platform.c，SCRCPT/cast-and-menu-source/S90f1-menu 和 menu-supervisor.sh。新板端候选 menu_graphics_boot_20261003，旧二进制和管理器仍在原目录，服务回退可从 Git 57ad956 提取原脚本。应用表保持最新57ad956投屏、设置、触摸和wiliwili入口不变。顺便纳入本地已有服务 alive() 对服务包装阶段的识别。

网络恢复：板子新网络wnl64，IP10.21.174.124；电脑仍在家庭网10.0.0.129，暂时无法连接wnl64。通过COM4的wpa控制添加临时家庭2.4G网络，利用电脑已保存密码在内存推导对应SSID的PSK；恢复板端SSH10.0.0.234。未保存临时网络、没有删除wnl64。旧5G哈希不能直接用于2.4G，因为PSK推导包含SSID；已纠正，日志不保存密钥。

验证计划：增量构建、源码提交后SCP部署并核验；菜单重复启动不重复进程；模拟内核日志后framebuffer保持；停止恢复文本模式、重启恢复菜单；冷启动、网络SSH回归。三轮完整依赖验收前不称稳定版。

首次增量构建暴露实体迁移后旧路径：platform.c 仍引用 ../ft6236_20261002/touch_orientation.h，Makefile 工具链也仍是旧目录。仅改为实际 ../touch 与现有 ROOTFS 下工具链；不新建链接、不更换工具链。构建仍在原 LVGL/menu 中增量执行。
