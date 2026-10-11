# F1C200S SDK 集成

当前状态：集成中，不是已验收的完整镜像 SDK。

统一目录根部入口：

```sh
./sdk doctor
./sdk build kernel
./sdk build uboot
./sdk build rootfs-sources
./sdk build rootfs-base
./sdk build board
./sdk build apps-ui
./sdk clean
```

`./sdk build` 为最终完整镜像入口。目前存在运行依赖未集成项，入口会明确拒绝生成不完整镜像。见 manifest.json 的 release_blockers；不能用旧二进制冒充全源码编译成功。

环境：Linux、Python 3.5+、Git、make、GCC、bison、flex、bc、OpenSSL 开发头文件、pkg-config。内核/U-Boot 使用 Linaro GCC 7.2.1。默认查找 TOOLCHAIN/linaro-7.2.1/bin，兼容当前虚拟机 /opt 下的原工具链；其他位置用 KERNEL_TOOLCHAIN=/absolute/bin 指定。不自动安装工具或修改主机环境。

构建从锁定 Git 提交导出源码，配置由 sdk-support/configs 提供，输出全部在 .sdk-output 下。不会在当前内核、U-Boot 原目录执行 clean 或覆盖产物。内核严格为5.7.1，显式传 LOCALVERSION=。

clean 仅删除有正确所有权标记的 .sdk-output，拒绝符号链接和未知目录，不删除当前开发目录原有的 Buildroot output、工具链、下载包、Git 历史。现有开发产物待完整 SDK 独立构建通过后再整理，不能提前删掉唯一依赖。

构建生成 images/<component>/build.json，记录提交、编译器、配置及产物 SHA256。构建成功不代表旧二进制逐字节一致或板子验收。当前不自动部署、不写 SD 卡、不停止投屏。

rootfs-sources 获取/校验基础 Buildroot 依赖；rootfs-base 在隔离目录完整构建基础系统与工具链，不代表最终运行镜像。唯一排除的已知旧 overlay 子模块仍在原目录保留且不丢弃其修改；排除引用也被锁定，其他源码修改照常拒绝。旧 overlay/post-build 不再用于此基础构建，避免重新收集历史二进制。libcedarc 锁定完整 Git 提交而不跟随浮动 master。

`./sdk clean --component rootfs` 只清基础系统的生成目录，保留其他已编译组件；默认 `./sdk clean` 仍清全部 SDK 生成数据。分组件清理同样检查输出所有权与原提交，拒绝未标记目录。

`python3 sdk-support/audit-buildroot.py` 对比官方精确2018.02.11发行包和现有源码，发行包放在 Buildroot 的 dl 缓存内，SHA256锁定。审计不覆盖定制文件。该工具已用于补回原目录缺失的9个 linux/集成文件；13个现有定制文件保持原样。

`python3 sdk-support/reproduce-uboot.py --source c5335a4` 用完整历史源码、原配置和板端测得的构建版本/时间重编 U-Boot，仅比较隔离产物与板端引导区 SHA256，不写 SD 卡。不同则失败，不认作对应原版。此历史核实必须在其他 SDK 构建结束后运行，共用构建/清理锁。

boot.cmd 来自当前板端启动脚本的文字内容，保留已用的内核/DTB地址、root分区和CMA20M参数。脚本中的旧HDMI参数也先原样保留，不代表实际RGB屏分辨率；最终DTB必须使用已核实的480×800厂商屏参源码。

board 从锁定的项目源码重新生成当前厂商屏参 DTB，并要求与板端 DTB 的 SHA256 完全一致；随后用隔离内核编译面板、电源、HIDP/UHID四个模块并检查5.7.1版本。apps-ui 用隔离工具链编译菜单、媒体控制界面、录音、阅读器、SFC和通用模拟器前端。它不是全部应用构建：ONS、wiliwili、媒体解码器和模拟器核心等仍需独立接入；两项目前待首次实编译，不代表板端验收。
