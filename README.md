# F1C200S 完整源码实体目录

根目录：`/home/wnk/Desktop/F1c200S_SOFT`。
本记录取代此前分类链接方案；以下主入口全部是真实目录，不是符号链接。

| 分类 | 实际源码目录 |
| --- | --- |
| KERNEL | linux-5.7.1：当前完整内核、配置和既有构建产物；historical：历史候选 |
| ROOTFS | buildroot-2018.02.11：完整 Buildroot、.config、dl、output |
| UBOOT | u-boot：原完整 U-Boot 仓库及构建产物 |
| DRIVER | panel-and-board：外置屏幕驱动；aic8800-ugreen-v14：完整原AIC源码包 |
| DRIVER | rtl8723-support：原RTL/H5支持仓库；cedar-ve-ion：外置Cedar/ION；historical：旧驱动源码 |
| SOFTWARE | cedar-player：完整播放器仓库；miracast-sink：完整接收器仓库；libcedarc：完整原目录 |
| SOFTWARE | existing-archives：既有归档实体目录；legacy-LicheePi_Nano / legacy-SoftWare：其余旧软件/资料原目录 |
| LVGL | menu：板端菜单源码；touch：触摸及测试界面源码，完整LVGL库在touch/lvgl |
| LVGL | wiliwili_lvgl：Windows迁入的最新LVGL版wiliwili完整源码及历史 |
| SCRCPT | cast-and-menu-source：CMA/菜单/投屏启动脚本源码 |
| SCRCPT | network-and-audio-source：BS联网、蓝牙、音频及外围支持源码 |
| SCRCPT | miracast-source-runtime：接收器原运行脚本、支持文件与既有快照 |

## Git 与完整性

原屏幕、触摸、菜单共用仓库的 .git 已实体移到统一根目录，原 Git 历史保留。
独立内核、Buildroot、U-Boot、AIC、RTL、播放器、接收器、wiliwili 的 .git 均保留在各自目录。
共享源码的路径变更作为移动提交记录，不重置未提交文件，不另开 Git worktree。
原有被修改的历史 DTB 保持未提交，不混入迁移提交。
原目录整体移动：源码、配置、未跟踪文件、构建文件一并保留，没有只挑部分源码。
主要目录 inode 及全部文件元数据移动前后核验一致；证据位于 records/*.before.files、*.after.files。
KERNEL/historical 中原有工作树的 Git 路径已经修复，没有新建工作树或删除历史。

内核 zImage SHA256：20e69f109abd3d8d95c73620c01287a82c17b10a5fbdb55020f0a419a5dfa4c9。
U-Boot产物 SHA256：9114f5f83ba816f0f9378f473c9aaf9141899a2c21aabc0a51ce23b4b204d290。
本轮不访问板子，不导出系统，不 clean/mrproper，不编译、不部署、不发布 GitHub。
既有归档只是迁移，不修改归档内文件、不创建新的备份。

## 路径规则

不再保留本轮创建的旧目录映射。以后使用上面的实体路径。
Linux源码中的内置RTL8723BS、RTC、TCA9555仍在完整内核的drivers目录内，
不搬出内核，也不在DRIVER下用映射冒充独立外置源码。
共享库/固件/工具链内部原有的文件符号链接保持原样，不属于分类映射。
历史记录中的旧路径仅作溯源，不能继续当作当前构建入口。
现有Makefile、SDK和生成缓存仍可能包含旧绝对路径；需逐项更新后再验证构建。
本次只完成实体搬迁与完整性核验，不宣称所有组件已可在新路径增量编译。
