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

目前已完整复现板端 U-Boot：源码c5335a4，程序版本时间与镜像头时间分别固定，完整SHA256为71e34730de102099163f014c528ca0656bb1ef5c81fc2756f6a32477cd3cac04。SDK正常U-Boot构建也导出此提交并检查相同哈希；不改原开发树的fafaf45 HEAD。

boot.cmd 来自当前板端启动脚本的文字内容，保留已用的内核/DTB地址、root分区和CMA20M参数。脚本中的旧HDMI参数也先原样保留，不代表实际RGB屏分辨率；最终DTB必须使用已核实的480×800厂商屏参源码。

board 从锁定的项目源码重新生成当前厂商屏参 DTB，并要求与板端 DTB 的 SHA256 完全一致；随后用隔离内核编译面板、电源、HIDP/UHID四个模块并检查5.7.1版本。apps-ui 用隔离工具链编译菜单、媒体控制界面、录音、阅读器、SFC和通用模拟器前端。它不是全部应用构建：ONS、wiliwili、媒体解码器和模拟器核心等仍需独立接入；两项目前待首次实编译，不代表板端验收。

Buildroot总包索引损坏也已修复；olddefconfig后必须仍选中基础系统必需包，生成tar后还会检查init、BusyBox、账户骨架、SSH、ALSA及蓝牙程序。只包含工具链的失败目标不会被当成有效rootfs。源码提交若只有变更记录更新，可刷新生成目录里的记录而保留编译缓存；代码变动则仍拒绝静默替换。

新增独立目标：`./sdk build ons` 构建完整无SDL的GBK引擎整数版及目录浏览器，Ogg/Vorbis/bzip2/Tremor依赖均由本地跟踪源重编，引擎快照必须逐文件匹配已有校验清单；`./sdk build retro-cores` 构建GBA/PS1/MD三个ARMv5 libretro核心，未包含游戏资源。以上目标已主机实编通过，尚未部署回归。

`./sdk build lan` 构建网页传输界面/HTTP服务和DLNA renderer，仅在应用打开后启动服务的策略由后续运行脚本整合，构建不代表服务常驻。该目标已主机实编通过，尚未实机验收。

`./sdk build media-backends` 已实编通过Cedar硬解播放器、无SDL MPlayer、wiliwili界面及网络播放后端。构建仅在生成目录执行，未部署板子。

`./sdk build miracast-sink` 从锁定的完整接收器Git源码重编FIFO/音频接收器，使用SDK新Buildroot工具链及ALSA，不复制历史二进制。它尚不包含AIC模块、WFD协议工具与启动服务整合。

`./sdk build bluetooth-audio` 从完整BlueALSA源重编服务、aplay及两个ALSA插件，并从跟踪源重编自动声卡路由。依赖新rootfs的SBC/GLib/D-Bus/BlueZ，安装只进入生成目录，尚不代表板端服务或镜像集成完成。

`./sdk build wireless-modules` 构建成功投屏版本的AIC loader/fdrv、小型RX预分配池、IP5209电源模块和RTL8723BS UART H5模块。使用隔离kernel及原Linaro7.2.1，严格要求5.7.1；AIC运行版和IP5209后续版分别锁定完整源码，不使用未接受的AIC接收路径候选。

`./sdk build wfd-protocol` 从固定SHA256的完整协议归档选择WFD版wpa_supplicant源码，排除历史二进制/对象，并重新编译supplicant、cli和passphrase。保留原WFD/P2P/AP配置，报告记录归档及配置哈希。已主机实编通过，不代表最终运行镜像已完成。

`./sdk build runtime-tools` 重编菜单使用的RTC/SNTP与静态IP工具，并运行同源码主机只读自测，不调用网络配置或授时写入功能。

`./sdk build runtime-payload` 按runtime-layout.json将重编程序/模块安装至生成runtime-payload目录，安装前逐个核对构建报告哈希，拒绝未知目录/重复目标/路径及符号链接逃逸。另从锁定源码安装字体、RTL/AIC固件、应用脚本和首批RTC/耳机/自动声卡路由服务；安装副本的路径替换必须匹配且通过shell语法检查，保存原始及安装哈希。此目标是最终rootfs集成的一步，完整启动链、共享库、基础系统及分区镜像仍待完成，不是可烧录镜像。

`./sdk build kernel-runtime` 按已编隔离kernel的modules.order导出全部配置模块，要求严格5.7.1，保存源/config/zImage/模块哈希。通用hci_uart由wireless-modules的外置H5替代；先构建wireless-modules再执行此目标。runtime-payload同时安装该完整模块集、普通WLAN/SSH/蓝牙加载服务、菜单与wiliwili启动文件。菜单调用的connections-helper-ip安装为同一重编helper的兼容名称，不复制历史程序。

LAN后端在生成源码副本中定点改为/opt/f1/apps/media/mplayer-native，编译后必须在二进制内找到该路径且无候选路径；原开发源码不修改。当前运行文件114项，不代表最终rootfs/固件完成，完整投屏启动链、硬件加载、共享库与SD镜像仍待整合。

`./sdk build rootfs-integrated` 校验并合并基础rootfs.tar与当前runtime-payload，保留特殊节点、链接与权限，运行文件统一root所有者；仅排除与专用WLAN冲突的全局S41dhcpcd启动脚本。合并后逐一检查所有ELF架构、加载器及NEEDED路径解析，失败不发布新归档，报告位于images/rootfs-integrated/dependencies.json。当前117运行文件合入后约192MiB、301个ARM ELF文件依赖检查通过；不代表ABI/运行验收，完整投屏入口与烧卡镜像仍未完成。

基础BusyBox配置在SDK副本中启用udhcpd，替代缺少C源的历史tiny_dhcpd_49；runtime-cast适配只在投屏启动时使用，把ACK转为既有watcher日志接口，租约和FIFO在/tmp。不在普通开机时启动DHCP服务器，此替代需要实机连接回归。

runtime-payload已加入正式投屏入口、AIC loader、PBC/RTSP watcher和内存准备脚本；源从锁定主仓库提交读取。原preflight校验仍保留，MD5由当前重编产物/前序安装文件生成，组装时再次离线逐项核对。运行层当前125文件，51测试通过，触摸退出helper的sink身份也在生成副本中迁移并重编。硬件启动/分区镜像/实际连接仍未验收，不代表最终烧卡固件完成。
