# 2026-10-03 菜单开机显示修复

## 计划 / pending

源码：/home/wnk/Desktop/F1c200S_SOFT，原分支 candidate/cma-start-preflight-20261002，基线 57ad956。原菜单二进制 SHA256 c3d363346434285edfef9285d8611059742030a9124ad3223f7cab8e93fe5ba4。保留原候选目录，不新建工作树、不改内核、驱动、播放器。

实测：COM4 115200，菜单 supervisor PID255、GUI PID263 从启动时正常运行；DRM 原生480x800输出 active=1。SSH恢复后读取 framebuffer，画面主要是内核日志、残留少量菜单标题，证实 fbcon 覆盖 GUI。platform.c 仅隐藏光标，没有 KD_GRAPHICS。修改为打开 tty0，保存 KDGETMODE，申请 KD_GRAPHICS，退出恢复原模式；失败明确输出错误。内核及串口日志不关闭。

文件：LVGL/menu/platform.c，SCRCPT/cast-and-menu-source/S90f1-menu 和 menu-supervisor.sh。新板端候选 menu_graphics_boot_20261003，旧二进制和管理器仍在原目录，服务回退可从 Git 57ad956 提取原脚本。应用表保持最新57ad956投屏、设置、触摸和wiliwili入口不变。顺便纳入本地已有服务 alive() 对服务包装阶段的识别。

网络恢复：板子新网络wnl64，IP10.21.174.124；电脑仍在家庭网10.0.0.129，暂时无法连接wnl64。通过COM4的wpa控制添加临时家庭2.4G网络，利用电脑已保存密码在内存推导对应SSID的PSK；恢复板端SSH10.0.0.234。未保存临时网络、没有删除wnl64。旧5G哈希不能直接用于2.4G，因为PSK推导包含SSID；已纠正，日志不保存密钥。

验证计划：增量构建、源码提交后SCP部署并核验；菜单重复启动不重复进程；模拟内核日志后framebuffer保持；停止恢复文本模式、重启恢复菜单；冷启动、网络SSH回归。三轮完整依赖验收前不称稳定版。

首次增量构建暴露实体迁移后旧路径：platform.c 仍引用 ../ft6236_20261002/touch_orientation.h，Makefile 工具链也仍是旧目录。仅改为实际 ../touch 与现有 ROOTFS 下工具链；不新建链接、不更换工具链。构建仍在原 LVGL/menu 中增量执行。

## 构建、部署及首轮冷启动结果

源码提交96b4c2c。构建命令 make -C /home/wnk/Desktop/F1c200S_SOFT/LVGL/menu -j2 f1-menu，原Buildroot GCC6.4；仅机械修正 build 下旧 .d 的迁移路径，未clean。rc=0。新GUI SHA256 bc7f3fd5c37771143038f11609c50897cf071c2b1eccf7b3ff488cc3291b5171；服务 adf6a5eb29b6c04e5f3fd7f79f9029ba7b2a02c40dfb43e816d85937f532c57f；管理器 e724a70d8f7804c9a4f4d7d58f179e6caf4b29861ef11bba2f695c1d55bfac36，均SCP传输后核验一致。apps.conf SHA256仍1897037c1c1e7cc2b2e017ca9baeab48bf04c8b3399dc7d4f3584cb7480666d1。新候选内connections-helper沿用原文件，中文字库通过硬链接共享原只读文件，没有额外RAM缓存。

运行结果：截图完整菜单；重复start保持同一supervisor，停止和恢复正常。写/dev/kmsg测试前后fb0 MD5均a0a288720b64b2765f44dc403dbab569，不再覆盖UI。网络两项已SAVE_CONFIG持久保存，wnl64优先级0，家庭2.4G优先级10；本条取代前面的“未保存临时网络”过程状态，家庭网存在时优先恢复SSH，外出仍可用wnl64。

冷启动1：先正常poweroff、COM3 OFF2确认/3秒/ON2确认，COM4捕获启动。boot_id75faa115-7d7e-41aa-bbd4-d91470eea675，开机约10秒启动菜单supervisor PID253；SSH10.0.0.234自动可用，wnk641_2.4G COMPLETED。开机日志注入前后fb0 MD5仍相同，截图菜单正常；MemAvailable38000KiB、CmaFree17692KiB。未动内核、驱动、播放器或投屏链路。暂未重新生成Buildroot镜像，属于板端持久部署。

冷启动2：boot_id a8beb893-bc68-4601-8e14-642703aab959，菜单supervisor PID259；冷启动3：boot_id798b510a-1f70-4bbb-8048-2901bcf2b66f，菜单supervisor PID259。两轮均先停止菜单/sync/poweroff，再用技能hub_power.py指定已确认COM3，取得OFF/ON准确回应并间隔3秒。两轮SSH均自动恢复，日志注入前后fb0 MD5均a0a288720b64b2765f44dc403dbab569。最终保留菜单运行，未启动测试播放器。

结论：三轮“菜单自启动+SSH+日志不覆盖菜单”的针对性冷启动检查通过；本轮未重测投屏/蓝牙播放/wiliwili播放依赖，完整稳定基线验收仍pending，不做稳定归档或GitHub稳定标记。一次性串口与测试包装程序测试后删除。回退从Git57ad956提取S90f1-menu恢复原服务，再指向保留的旧候选目录。
