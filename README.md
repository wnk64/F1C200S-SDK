# F1C200S SDK

统一管理 F1C200S 当前使用的 Linux 5.7.1、U-Boot、Buildroot、AIC8800/RTL8723BS 支持、LVGL、投屏、wiliwili、本地媒体、阅读器、ONS、模拟器及板端脚本。

## 下载与配置

整个项目只使用这一个 GitHub 仓库。`main` 保存 SDK 入口、配置、应用和脚本；`source/*` 分支保存独立组件的完整源码和 Git 历史，版本锁保存在 `sdk-support/manifest.json` 和 `sdk-support/github-sources.json`。不是指向已删除旧仓库的 submodule。

```sh
git clone --single-branch --branch main git@github.com:wnk64/F1C200S-SDK.git
cd F1C200S-SDK
gh release download sdk-source-20261013 -R wnk64/F1C200S-SDK -p dependencies-20261013.tar.gz
python3 sdk-support/github_sources.py --dependencies dependencies-20261013.tar.gz
./sdk doctor
```

私有仓库需要授权 Git SSH 和 GitHub CLI。依赖包包含 Linaro 7.2.1 工具链、Buildroot 下载缓存、锁定基线检查文件和协议源码归档，不含 `.sdk-output` 编译缓存、用户 ROM 或媒体。还原程序核验 SHA256，不覆盖不同内容的已有文件，不创建额外 SDK 副本。配置完成后可删除下载的压缩包。

已验证主机环境为 Ubuntu 16.04.7 x86_64，Python 3.5.2、Git 2.7.4、GCC 5.4.0。主机还需 make、gcc/g++、bison、flex、bc、autoconf、automake、libtool、pkg-config、OpenSSL/ncurses 开发包、cpio、unzip、rsync、wget、mtools、dosfstools。其他发行版兼容性尚未完整验证。

## 编译

```sh
./sdk build -j8                  # 只增量编译，不组装镜像
./sdk build apps-ui -j8          # 只增量编译一个组件
./sdk build --image -j8          # 增量编译后组装 SD 镜像
./sdk clean                     # 显式清理生成数据，不删除源码
```

输出位于 `.sdk-output`。固件为 `.sdk-output/images/sd-image/f1c200s-sdk-candidate.img`。不自动烧卡或部署。详细构建策略见 [BUILD_POLICY.md](sdk-support/BUILD_POLICY.md)。

## 当前状态

包括 DLNA 内存/退出回收修复、所有菜单应用统一退出资源回收，以及显式镜像打包和增量缓存策略。主机构建入口已通过 80 项自动测试；应用生命周期完成 11 轮板端测试。最新候选仍缺完整音视频回归及三次物理冷启动验收，不能视为已验收稳定发布。此前完整独立构建记录仅证明其对应旧提交，不冒充最新提交的全量构建验收。

第三方源码保留各组件原有许可证及版权声明；本仓库汇总不改变其许可证。历史开发记录可能包含本地路径、设备地址和实验说明，公开前需继续审查。
