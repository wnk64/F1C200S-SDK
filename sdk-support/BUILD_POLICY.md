# SDK 编译与镜像打包

默认命令只编译，保留构建目录并复用未变的组件，不清理、不组装 rootfs、不生成 SD 镜像：

```sh
./sdk build -j8
./sdk build apps-ui -j8
./sdk build runtime-tools -j8
```

需要固件时明确添加参数：

```sh
./sdk build --image -j8
```

仅打包已有产物也需要显式授权，且不会自动补编缺失的依赖：

```sh
./sdk build runtime-payload --image
./sdk build rootfs-integrated --image
./sdk build sd-image --image
```

增量记录位于 `.sdk-output/stamps/incremental`。源锁、配置、构建脚本、依赖报告和产物状态未变时输出 `UP TO DATE`；否则调用该组件原有构建流程，保留 Make/Buildroot 的对象文件。首次启用缓存需执行一次组件构建建立成功记录，不会将旧产物直接认作新缓存。失败不写成功记录，产物缺失或变化会重新构建。构建作业数不参与源码缓存失效。

源码仍受 manifest 版本锁保护；修改版本锁需遵守源码审核流程，不能借缓存绕过检查。`./sdk clean` 仍是显式清理命令，下一次构建自然需要从头编译。原有 README 中将 `./sdk build all` 描述为直接生成镜像的用法，统一以本文为准，须改用 `./sdk build --image`。

本变更只调整主机构建入口，不修改板端程序或固件内容。
