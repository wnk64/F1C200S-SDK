# F1C200S 开发目录整理（2026-10-02）

目标：Ubuntu `/home/wnk/Desktop/F1c200S_SOFT`。
分类：KERNEL、ROOTFS、UBOOT、SCRCPT、DRIVER、SOFTWARE、LVGL。

## 计划

只迁移原有目录，不新建 Git worktree，不删除历史资料，不改源码行为。
独立仓库整体移动，旧绝对路径保留符号链接，以兼容现有构建文件。
共享仓库中的子目录保留原相对结构，用分类链接提供入口。
迁移前后核对提交、已跟踪文件状态、关键产物校验和。

板端当前为 Linux 5.7.1 #250、CMA 20 MiB。
板端 FAT zImage 与虚拟机原内核 zImage SHA256 均为：
`20e69f109abd3d8d95c73620c01287a82c17b10a5fbdb55020f0a419a5dfa4c9`。
本次不编译、不部署、不重启、不停止正在运行的投屏。

ROOTFS 只整理虚拟机的完整 Buildroot 开发目录，不导出板端系统。
此前误启动了板端 rootfs 读取，已停止并删除本轮生成的导出文件。
联网配置及密钥属于私有资料，不上传 GitHub。
U-Boot 在与 SD 卡启动区域核对前，只标记现有源码/产物，不能宣称匹配当前板端。
AIC 当前工作树 HEAD 含后续候选；实际在用驱动源码参考已记录的 53bec9e 基线。

## 结果

已完成原仓库迁移，七个独立仓库的 HEAD 与全部未提交状态迁移前后完全一致；
共享 LicheePi_Nano 仓库的已跟踪文件状态一致。内核 .config、vmlinux、System.map、
zImage 和 U-Boot 产物 SHA256 验证通过。没有新建 git worktree。

Buildroot 完整目录约 6.9 GiB；Makefile、Config.in、package、board、.config、dl、
output/host、output/build、output/target 均存在，没有按白名单裁剪。
`.config` SHA256：`9b7f12245973997e63e4d18f2582e81de6291de771c03864c04a106ebe8c0455`。
属于共享仓库的 Buildroot/U-Boot 保留原相对路径，分类入口使用链接，避免破坏 Git。

Windows wiliwili LVGL 源码完整迁入 LVGL/wiliwili_lvgl，排除 Windows 构建目录，
保留原 Git 历史和最新未提交代码，保存提交 `39695b1`。
不改 Windows 原文件，不运行该程序，不宣称 ARM 移植完成。
旧版 library 子模块保留原 Git 引用，实际依赖源码一并迁入；本轮不更新子模块。

本轮没有修改板子文件或停止投屏。误生成的虚拟机 board-current、board-boot、
running-rootfs.tar.gz 已删除，原板端数据保留。临时传输包和命令文件执行后清理。

## 分类入口

以下均相对 `/home/wnk/Desktop/F1c200S_SOFT`：

| 分类 | 入口 | 说明 |
| --- | --- | --- |
| KERNEL | linux-5.7.1 | 原内核完整仓库及现有增量构建产物 |
| ROOTFS | buildroot-2018.02.11 | 完整 Buildroot；链接到 LicheePi_Nano 内的原目录 |
| ROOTFS | LicheePi_Nano | 共享原仓库，历史资料及相对结构不拆散 |
| UBOOT | u-boot | 原 U-Boot 仓库入口；保留共享仓库已跟踪文件 |
| SCRCPT | cast-and-menu-source | 当前 CMA/菜单/投屏脚本源码 |
| SCRCPT | network-and-audio-source | BS 联网、蓝牙、音频路由与外围脚本源码 |
| SCRCPT | miracast-source-runtime | 接收器原仓库中的脚本与运行支持文件 |
| DRIVER | panel-and-board | 屏幕、触摸、显示参数及当前菜单共享仓库 |
| DRIVER | aic8800-ugreen-v14 | 原 AIC 源码包；不能把当前候选 HEAD 当作在用驱动 |
| DRIVER | rtl8723-support | 原 RTL 支持/外置 H5 蓝牙及音频路由仓库 |
| DRIVER | rtl8723bs-wifi | 当前内核的 BS Wi-Fi 源码入口 |
| DRIVER | cedar-ve-ion | 原外置 Cedar/ION 文件目录；原本不是独立 Git 仓库 |
| DRIVER | rtc-pcf8563.c / tca9555-gpio.c | 当前内核内置外围驱动入口 |
| SOFTWARE | cedar-player | 原 Cedar 播放器完整仓库 |
| SOFTWARE | miracast-sink | 原接收器完整仓库 |
| SOFTWARE | existing-archives | 原归档位置入口，不创建新归档 |
| LVGL | menu / touch / lvgl-8.2.0 | 当前菜单、触摸与 LVGL 源码入口 |
| LVGL | wiliwili_lvgl | 从 Windows 迁入的最新 LVGL 重写版完整源码 |

旧 /home/wnk/LicheePi_Nano、/home/wnk/tk032f8004-panel 等入口仍可用。
不要删除这些兼容链接：现有 Makefile、构建缓存和历史 Git 工作树仍引用旧路径。
以后开发从上述分类入口进入；本轮不完整重编，也不发布到 GitHub。
