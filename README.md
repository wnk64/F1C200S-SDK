# F1C200S SDK

版本：`0.2.0-rc1`，候选版本，尚未完成新镜像实卡首次启动和三次冷启动验收。`./sdk --version` 查看版本。

统一管理 F1C200S 当前使用的 Linux 5.7.1、U-Boot、Buildroot、AIC8800/RTL8723BS 支持、LVGL、投屏、wiliwili、本地媒体、阅读器、ONS、模拟器及板端脚本。

完整中文步骤见 [使用说明](sdk-support/USER_GUIDE.md)：主机依赖安装、首次下载还原、增量编译、显式打包、备用SD卡烧录校验、源码及版本锁维护、清理和排错。已有开发目录原地继续使用，不需要再复制SDK。

## 下载与配置

整个项目只使用这一个 GitHub 仓库。`main` 保存 SDK 入口、配置、应用和脚本；`source/*` 分支保存独立组件的完整源码和 Git 历史，版本锁保存在 `sdk-support/manifest.json` 和 `sdk-support/github-sources.json`。不是指向已删除旧仓库的 submodule。

```sh
git clone --single-branch --branch main https://github.com/wnk64/F1C200S-SDK.git
cd F1C200S-SDK
wget https://github.com/wnk64/F1C200S-SDK/releases/download/sdk-source-20261013/dependencies-20261013.tar.gz
python3 sdk-support/github_sources.py --dependencies dependencies-20261013.tar.gz --repository https://github.com/wnk64/F1C200S-SDK.git
./sdk doctor
```

仓库已经过公开前凭据检查并设置为公开，上述 HTTPS 下载不需要 GitHub 登录。依赖包包含 Linaro 7.2.1 工具链、Buildroot 下载缓存、锁定基线检查文件和协议源码归档，不含 `.sdk-output` 编译缓存、用户 ROM 或个人媒体。还原程序核验 SHA256，不覆盖不同内容的已有文件，不创建额外 SDK 副本。配置完成后可删除下载的压缩包。原有开发环境不用重新下载或执行还原，直接继续使用原目录。

已验证主机环境为 Ubuntu 16.04.7 x86_64，Python 3.5.2、Git 2.7.4、GCC 5.4.0。主机还需 make、gcc/g++、bison、flex、bc、autoconf、automake、libtool、pkg-config、OpenSSL/ncurses 开发包、cpio、unzip、rsync、wget、mtools、dosfstools。其他发行版兼容性尚未完整验证。

## 编译

```sh
./sdk build -j8                  # 只增量编译，不组装镜像
./sdk build apps-ui -j8          # 只增量编译一个组件
./sdk build --image -j8          # 增量编译后组装 SD 镜像
./sdk clean                     # 显式清理生成数据，不删除源码
```

输出位于 `.sdk-output`。固件为 `.sdk-output/images/sd-image/f1c200s-sdk-candidate.img`。不自动烧卡或部署。详细构建策略见 [BUILD_POLICY.md](sdk-support/BUILD_POLICY.md)。

注意：同一版本锁下复用构建缓存，不等于任意源码跨版本都已支持无清理增量迁移。当前独立组件代码换版、共享源码修改/删除仍存在迁移保护限制，详见[源码及版本锁维护](sdk-support/USER_GUIDE.md#6-修改源码与版本锁)。不要为了绕过检查直接删缓存或改stamps。

## 当前状态

包括 DLNA 换片状态同步、MPlayer 显示/旋转帧预留、wiliwili 拖动预滚死锁与返回封面补齐、媒体触摸修正，以及统一应用退出回收。新增首启 FAT32 数据分区：新镜像为 512 MiB，烧入至少 1 GB 的 SD 卡后，首次启动使用剩余空间建立 `/root/roms`，成功后自动删除初始化脚本并重启一次。已有第三分区的卡不会自动格式化。FAT32 单文件最大 4 GiB-1，不能存单个超过此限制的大文件。详见使用说明。已知个别视频仍可能卡顿，不能视为全部问题已解决；最新候选仍缺完整音视频回归及三次物理冷启动验收。

第三方源码保留各组件原有许可证及版权声明；本仓库汇总不改变其许可证。历史开发记录可能包含本地路径、设备地址和实验说明，公开前需继续审查。

公开前检查记录见 [PUBLICATION_AUDIT.md](sdk-support/PUBLICATION_AUDIT.md)。默认账户和实验网络配置仅用于开发；部署前应更改默认密码，不要直接暴露 SSH 或文件传输服务到公网。
