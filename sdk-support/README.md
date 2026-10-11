# F1C200S SDK 集成

当前状态：集成中，不是已验收的完整镜像 SDK。

统一目录根部入口：

```sh
./sdk doctor
./sdk build kernel
./sdk build uboot
./sdk build rootfs-sources
./sdk build rootfs-base
./sdk clean
```

`./sdk build` 为最终完整镜像入口。目前存在运行依赖未集成项，入口会明确拒绝生成不完整镜像。见 manifest.json 的 release_blockers；不能用旧二进制冒充全源码编译成功。

环境：Linux、Python 3.5+、Git、make、GCC、bison、flex、bc、OpenSSL 开发头文件、pkg-config。内核/U-Boot 使用 Linaro GCC 7.2.1。默认查找 TOOLCHAIN/linaro-7.2.1/bin，兼容当前虚拟机 /opt 下的原工具链；其他位置用 KERNEL_TOOLCHAIN=/absolute/bin 指定。不自动安装工具或修改主机环境。

构建从锁定 Git 提交导出源码，配置由 sdk-support/configs 提供，输出全部在 .sdk-output 下。不会在当前内核、U-Boot 原目录执行 clean 或覆盖产物。内核严格为5.7.1，显式传 LOCALVERSION=。

clean 仅删除有正确所有权标记的 .sdk-output，拒绝符号链接和未知目录，不删除当前开发目录原有的 Buildroot output、工具链、下载包、Git 历史。现有开发产物待完整 SDK 独立构建通过后再整理，不能提前删掉唯一依赖。

构建生成 images/<component>/build.json，记录提交、编译器、配置及产物 SHA256。构建成功不代表旧二进制逐字节一致或板子验收。当前不自动部署、不写 SD 卡、不停止投屏。

rootfs-sources 获取/校验基础 Buildroot 依赖；rootfs-base 在隔离目录完整构建基础系统与工具链，不代表最终运行镜像。唯一排除的已知旧 overlay 子模块仍在原目录保留且不丢弃其修改；排除引用也被锁定，其他源码修改照常拒绝。旧 overlay/post-build 不再用于此基础构建，避免重新收集历史二进制。libcedarc 锁定完整 Git 提交而不跟随浮动 master。
