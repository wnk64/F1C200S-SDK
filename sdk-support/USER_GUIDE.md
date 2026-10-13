# F1C200S SDK 使用说明

本说明对应当前 `main`。SDK 在 Linux 主机编译，不是在板子上编译。已有开发目录 `/home/wnk/Desktop/F1c200S_SOFT` 继续原地使用，不必重新下载、复制SDK或清理现有缓存。

## 1. 主机环境

已经验证 Ubuntu 16.04.7 x86_64、Python 3.5.2、Git 2.7.4、GCC 5.4.0；新版Ubuntu/WSL/容器尚未完整验证。Ubuntu16.04已停止普通支持，建议使用隔离开发环境；若apt源失效，先按Ubuntu官方说明配置历史源或有效的扩展支持源，不要绕过软件包签名检查。

在准备好的Ubuntu主机安装主机构建依赖（只安装一次）：

```sh
sudo apt-get update
sudo apt-get install -y build-essential git python3 wget ca-certificates \
    bison flex bc gperf autoconf automake libtool pkg-config \
    libssl-dev libncurses5-dev libncursesw5-dev \
    cpio unzip zip rsync file patch perl tar gzip bzip2 xz-utils \
    mtools dosfstools e2fsprogs device-tree-compiler
```

这不是ARM交叉编译器的安装命令。内核/U-Boot使用的Linaro7.2.1随依赖包提供；应用工具链由锁定Buildroot生成，不要随意替换系统里的ARM工具链。以上apt步骤本轮只编写说明，没有在现有主机执行安装。

磁盘建议至少留出30GiB可用空间；之前完整构建的生成数据约23GiB，具体占用取决于缓存与输出。源码和依赖另占空间。不要因为磁盘占用就重复下载SDK或直接删除原始源码。

## 2. 首次下载与还原

仅新主机需要执行：

```sh
git clone --single-branch --branch main https://github.com/wnk64/F1C200S-SDK.git
cd F1C200S-SDK
wget https://github.com/wnk64/F1C200S-SDK/releases/download/sdk-source-20261013/dependencies-20261013.tar.gz
printf '%s\n' 'ac740360e6321d88e6191a3ccc5bb89293083cbee54ab196dd64473ba71234b0  dependencies-20261013.tar.gz' | sha256sum -c -
python3 sdk-support/github_sources.py \
    --dependencies dependencies-20261013.tar.gz \
    --repository https://github.com/wnk64/F1C200S-SDK.git
./sdk doctor
```

GitHub网页的“Download ZIP”只包含当前分支，不包含完整组件Git历史，不能代替上述流程。`source/*`和`sdk-lock/*`均属于同一个仓库，不依赖旧F1C200S仓库。还原器会取回组件历史并checkout版本锁；发布分支清理CI的提交不会改变实际构建基线。

依赖包约652MiB，包含工具链、Buildroot下载缓存和必需的基线/协议文件；没有20多GiB编译缓存。还原器检查SHA256，遇到不同内容的已有文件就拒绝覆盖。不要在已有开发目录再次解包、运行首次还原或手动覆盖源码。确认还原及doctor通过后，下载压缩包可自行删除，不要删除已还原的TOOLCHAIN或Buildroot dl目录。

## 3. 日常增量编译

```sh
./sdk build -j8
```

默认编译，不打包镜像，不自动clean。相同版本锁、配置、构建脚本、依赖报告和产物状态下，未变组件输出`UP TO DATE`；需要重建的组件进入现有构建流程，复用可用的Make/Buildroot对象。首次构建或显式clean之后当然需要完整构建，不存在没有初始对象的增量编译。

只编译指定组件：

```sh
./sdk build apps-ui -j8          # 菜单及UI子集
./sdk build runtime-tools -j8    # SDK生成的运行工具
./sdk build lan -j8              # DLNA/网页传输应用
./sdk build media-backends -j8   # 媒体及wiliwili后端组
./sdk build ons -j8
./sdk build retro-cores -j8
./sdk build miracast-sink -j8
./sdk build kernel -j8
./sdk build uboot -j8
./sdk build rootfs-base -j8
```

组件命令不是自动解析所有前置依赖的包管理器。例如新环境只编译apps-ui时，工具链可能还不存在；首次应执行`./sdk build -j8`，之后再用单组件命令。`./sdk build --help`给出实际支持的组件名；没有独立`build wiliwili`命令。

当前“增量”保证的是既有锁定构建状态的复用，并不是已经实现任意源码版本的自动迁移。修改源码及版本锁的限制见第6节，不能承诺任何文件改完都能直接重编。

## 4. 显式打包镜像

```sh
./sdk build --image -j8
```

此命令先增量编译，再组装运行文件、rootfs和SD镜像。仅已有编译产物需要重新组装时，可顺序执行：

```sh
./sdk build runtime-payload --image
./sdk build rootfs-integrated --image
./sdk build sd-image --image
```

这些单组件打包命令不补编缺失的前置产物，缺依赖会报错。产物不完整或不确定时使用`./sdk build --image -j8`。不带`--image`调用上述打包组件会拒绝执行。

镜像位置：

```text
.sdk-output/images/sd-image/f1c200s-sdk-candidate.img
```

该文件目前为1GiB的完整SD卡镜像（含引导区和分区），不是单独kernel/rootfs，也不是SPI NAND固件。主机编译、打包和烧卡是三步；SDK不会自动覆盖SD卡。

## 5. 烧入备用SD卡

全卡写入会覆盖目标卡的分区和已有数据。只能选择明确可覆盖的备用卡，不要直接覆盖现用系统卡、主机系统盘或USB硬盘。至少能容纳当前1GiB镜像，建议使用2GiB或更大SD卡。此处仅提供步骤，本轮没有执行烧录。

Windows主机：将镜像传到Windows，在balenaEtcher等镜像写入工具中选择该`.img`，核对读卡器和SD卡容量，写入并完成校验，然后安全弹出。系统提示格式化Linux分区时取消，不要格式化。

Ubuntu主机：先插卡，通过型号、容量、连接方式和挂载点确定整卡设备：

```sh
lsblk -o NAME,SIZE,MODEL,TRAN,MOUNTPOINT
```

下面以**已经人工确认**的备用卡`/dev/sdX`为示意。必须替换为真实整卡设备，不是`/dev/sdX1`等分区；未核对时不要执行。先用文件管理器卸载目标卡的所有已挂载分区，再操作。不要复制未解析的设备名或猜测设备字母。

```sh
image=.sdk-output/images/sd-image/f1c200s-sdk-candidate.img
sha256sum "$image"
# Only after verifying the exact disposable SD card device:
sudo dd if="$image" of=/dev/sdX bs=4M status=progress conv=fsync
sync
sudo cmp -n "$(stat -c %s "$image")" "$image" /dev/sdX
```

`cmp`无输出且退出状态为0表示镜像长度范围内逐字节一致；报错时不要把写入视为成功。安全弹出卡后再插板子，断电再上电。首次验收至少检查菜单/触摸、联网与SSH、蓝牙音频、投屏和应用退出回收。串口参数115200、8N1，COM编号按实际枚举，不沿用历史端口。最新候选仍需三次真实断电冷启动及完整业务回归，软件重启不计作冷启动验收。

## 6. 修改源码与版本锁

SDK不是直接编译所有工作目录当前未提交文件。它从锁定Git提交导出到`.sdk-output`构建；只修改源码或只改manifest中的一个hash，都不能视作已经纳入固件。

先确认文件属于哪个Git仓库，禁止`git add .`混入无关改动：

```sh
git status --short
git -C LVGL/wiliwili_lvgl status --short
git -C SOFTWARE/cedar-player status --short
git -C KERNEL/linux-5.7.1 status --short
```

目录分类：

| 修改范围 | 实际版本管理/锁 |
| --- | --- |
| sdk-support中的生成器、runtime-menu等 | 主仓库已提交源码；由构建配方和输入摘要检查 |
| LVGL/menu、媒体UI、ONS/模拟器前端、板端共享脚本等 | 主仓库中的`project_sources.commit`及`paths` |
| 内核/U-Boot/Buildroot/播放器/wiliwili等独立仓库 | `manifest.json`中对应`components`的`commit`/`export_commit` |
| 新主机组件还原 | `github-sources.json`中的路径、分支、`head`和`commits` |

维护流程：

1. 只改一个功能，在其实际仓库中检查diff、测试、提交源码，更新中文及工作树记录，保留可回溯旧提交。
2. 主仓库共享源码修改后，更新`project_sources.commit`为包含该修改的完整主仓库提交；独立组件则更新对应锁。修改配置时同步配置副本与`config_sha256`。存在`export_commit`时，真正导出编译的是它，不只是`commit`，两者不能误认为相同。
3. `baseline_sha256`是已知原运行/检查材料的哈希，`expected_output_sha256`是特定重现目标的输出约束，不能为了通过检查随便填成新文件哈希。内核配置、版本、原检查材料和对应完整源码关系必须先明确；内核模块仍锁定5.7.1，不在本说明中授权修改硬件配置或重建稳定基线。
4. 对需要发布的组件，将新源码提交/版本锁推送到统一仓库相应分支和锁定引用，同时同步`github-sources.json`。若基线/工具链/协议材料改变，还需新的依赖资产、SHA256和发布记录，不能覆盖旧依赖包却保留旧摘要。
5. 版本锁和生成源码记录也需提交到主仓库，先跑`./sdk doctor`与相关测试，再只构建受影响组件。重编成功不自动证明新代码部署或硬件验收成功。

### 当前版本锁迁移限制

这里明确记录真实实现，不提供不存在的“一键解锁”命令：

- 独立组件代码提交变了、已有隔离导出仍是旧提交时，当前`stage()`会报`Staged code changed; clean this component before rebuilding`。这是代码版本迁移保护，并非普通日常构建会自动清缓存。经维护者确认需要重建该组件时，才显式用对应manifest键，例如`./sdk clean --component wiliwili`；随后重建实际包含它的`media-backends`组。不会因此清整个SDK，但被清组件本身需要重编。
- 主仓库共享源码目前只实现了经过审核的“纯新增文件”迁移；修改/删除既有文件没有通用增量刷新入口，会报`Unreviewed project source migration`等错误。不能只改`project_sources.commit`就承诺普通跨版本增量成功，也不能手改stamps伪装已迁移。应先做对应的受控迁移支持；本轮补文档没有实现该功能，也没有为绕过它执行全局clean。
- 不修改版本锁而直接改`.sdk-output/work`不是正式源码维护方式，可能产生无法复现的候选，不应作为发布流程。
- 单组件构建目前也不自动重建所有反向依赖。维护者需识别受影响的UI、后端、库、驱动和打包步骤；需要新镜像时最后显式`--image`。

因此，SDK当前适合锁定版本的可复现编译与同版本缓存复用；任意源码版本的无清理增量迁移仍有欠缺。不要把这个限制藏在“改完直接一条命令”描述里。

## 7. 清理与恢复

```sh
./sdk clean
```

只在明确要恢复无编译输出状态时执行。它删除带有效所有权标记的整个`.sdk-output`，包括该目录内的镜像、导出包和报告；保留正式源码、Git历史、还原的工具链和原始下载依赖。清理前确认里面是否还有唯一需要保留的产物，清理本身不会给你创建额外备份。

普通小改动不要先clean。`./sdk clean --component NAME`只适用于`./sdk clean --help`列出的独立manifest组件，不能把所有build子命令名都当作clean组件名。被清组件及依赖缓存需要在后续构建时重新检查。

失败时保留报错和对应build.json，不要直接mrproper、改stamps、删源码或替换未知工具链。记录常见错误：

| 报错/现象 | 处理 |
| --- | --- |
| Another SDK command is running | 检查已有SDK进程，不并行启动冲突构建/清理 |
| Missing host tool / Linaro missing | 补主机依赖或确认依赖还原/工具链路径 |
| 缺应用交叉编译器 | 先完成rootfs-base/初次默认编译 |
| Project source changed / Unreviewed source commit | 检查正确仓库和锁定版本，按第6节处理，别清空源码 |
| packaging requires explicit --image | 只有真正需要打包时才添加--image |
| Refusing unmarked output directory | 停止，不绕过所有权检查或直接递归删除 |
| kernel release mismatch | 核对5.7.1版本锁与对应源码/模块，不强行部署 |

GitHub Actions已关闭，发布分支的4个上游多平台工作流已移除。它们不是F1C200S构建步骤；旧失败邮件不表示本地SDK编译失败。
