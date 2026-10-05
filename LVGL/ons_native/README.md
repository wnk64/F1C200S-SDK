# ONS 原生后端移植设计

目前完整原 ONS 执行器已链接并在板端运行，用户确认《沙耶之歌》标题、触摸推进及声音可以工作；不再仅是资源预览。仍未完成全游戏兼容、存读档与长期稳定验收。沿用 CN/GBK 脚本核心，不重写少量命令模拟器。旧 ONS 及 SDL 候选保留，不修改系统 SDL。

最新状态与构建记录见 BASELINE_20261005.md 和 变更记录.md。下文早期分阶段说明保留为设计历史，不能当作当前完整执行器状态。浮点版 make ons-lvgl 已成功，完整13个执行源、原文字/特效/存读档算法均纳入；多路混音自测通过，120秒用户测试零欠载，但后续标题页90秒出现1欠载。bg1未支持，存读档及三冷回归尚未验证。

整数解码独立构建：make -f Makefile.tremor -j2 ons-lvgl-tremor native-runtime-mixer-tests-tremor。当前整数候选因真实短音效多解出176帧未通过断言，禁止替代原浮点版；尚无整数版CPU改善测量。

## 分层

| 功能 | 原依赖 | 目标/当前进度 |
|---|---|---|
| 脚本、标签、变量、存读档 | 现有ONS核心 | 原ScriptHandler/ScriptParser与基本命令已链接；游戏37条定义实测；正文图形执行及存读档待接入 |
| NSA资源 | ONS读取接口 | 原生索引读取、大小写/斜杠匹配、LZSS已验证；SPB/NBZ/加密包待实现 |
| JPEG/PNG/BMP | SDL_image | libjpeg/libpng/原生BMP已验证；BMP目前BI_RGB的8/24/32位 |
| 像素缓冲、透明合成 | SDL_Surface/Blit | 原AnimationInfo完整绘制/变换代码已编译，合成/帧切换/动画/字形实测；原图形命令/特效接入待做 |
| 中文字体 | SDL_ttf | 原FontInfo已接FreeType句柄、度量、加粗与描边；字形通过原文字合成实测，无需系统gconv |
| 显示、触摸 | SDL video/event | 正文运行时LVGL8.2/FT6236绝对触摸/局部刷新已实现；完整引擎实机显示待链接 |
| 计时、线程 | SDL timer/thread | 正文运行时128项筛选队列/独立32项timer线程已板端自测；完整引擎回归待做 |
| OGG与混音 | SDL_mixer/SMPEG | 单路OGG/NSA范围流/ALSA/暂停音量已实测；多路音效混音及命令接入待做 |

## 内存与刷新原则

- 图像buffer由外部分配器管理，不塞进LVGL的128KiB控件堆。所有surface有尺寸/像素数上限，解包有压缩/展开长度上限。
- 不为每个精灵永久复制整屏。基础画布、文字/特效缓冲按生命周期持有；资源解码后释放输入，缓存数量/字节数需要固定预算。
- 后续文字、按钮和精灵变化只提交实际脏矩形；30/60Hz不是强制整屏重绘，不应逐字符刷新整个480x800。
- 90度旋转输出直接读ARGB像素到LVGL条带，不调用逐像素SDL_GetRGB，不另建全屏旋转缓冲。
- 预览里的800x600到640x480拟合仅用于独立资源测试，不能替代脚本引擎的坐标/裁剪/缩放策略。
- 音频线程与显示线程分开；不给显示加载持有的锁阻塞音频回调。是否流畅必须实测，去SDL本身不是流畅保证。

## 构建与检查

```sh
cd /home/wnk/Desktop/F1c200S_SOFT
make -C LVGL/ons_native -j2
ROOTFS/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-readelf -d LVGL/ons_native/ons-native-probe
ROOTFS/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-nm -u LVGL/ons_native/ons-native-probe
```

`libons-native.a`提供native.h中的图像/字体/NSA接口，无SDL头文件或链接选项。生成GBK码表只在主机构建时用Python标准编码库；板端不需要Python或iconv模块。

`libons-script.a`直接编译原ONS的未改ScriptHandler.cpp；source-baseline.sha256锁定引用源，校验失败则停止构建。调用顺序必须是构造、reset()、setKeyTable(NULL)、openScript()，不能省略原引擎要求的初始化。`ons-script-probe`仅验证词法/标签/表达式，不执行图像和声音命令。

`libons-commands.a`编译原ScriptParser/ScriptParser_command和原Nsa/Sar/DirectReader、AnimationInfo、FontInfo、DirtyRect、resize_image。prepare_engine.py校验哈希后，在build/engine生成原生类型版本：没有SDL头文件/运行库，不改原源。AnimationInfo原绘制/变换/动画方法已纳入；仅透明预处理和alpha命中改为带边界检查的原生实现。FontInfo原布局代码保留，openFont通过16项路径/实际字号缓存接FreeType。生成器保留原版权和GPL声明，是需保留的正式构建工具，不是临时诊断脚本。

`ons-command-probe`调用原命令实现验证整数运算、字符串与别名，并从游戏*define执行37条真实命令至game边界，确认3个用户子程序注册。原NSA资源读取后交原生图像解码器。探针遇到未覆盖定义命令明确报错，不忽略未知命令；它不是新的部分游戏解释器，不执行game/正文、不写存档。

events.h提供64项有界线程安全队列及16项单调计时器。队列满时返回错误，由调用方处理，不能静默丢弃松开/退出事件。先close唤醒线程，再join所有使用者，最后destroy；测试目标native-event-tests验证这些基础路径。

sprite.h实现原生精灵基础处理。`ons_sprite_open`复制源并应用指定透明规则；`ons_sprite_adopt`仅用于无需转换的已解码图像，接管所有权且清空源指针，不多复制一张整图。两种初始化只能在空精灵上调用，使用后`ons_sprite_close`释放。每帧右半边遮罩要求cell宽度为偶数；非法cell/尺寸/遮罩明确失败。`ons_sprite_draw`的placement与clip均为目标画布坐标，dirty仅包含像素发生变化的区域；缩放横向映射固定上限2048项，每行复用，避免ARM逐像素整数除法。

surface.c提供带pitch和通道mask的CPU像素缓冲，8位字形/16位RGB565/32位像素格式可分配；原绘制算法直接访问这些缓冲。lock/unlock只验证并返回，因缓冲由单渲染线程独占，不是硬件设备表面；不能以此保护多线程修改同一像素缓冲。现有native/sprite工具的像素数组操作以标准ARGB8888为前提，不能把RGB565/8位buffer传给它们当32位图片使用。

font.h提供FreeType句柄、度量、字形和描边，native-font-tests验证原FontInfo -> FreeType -> 原AnimationInfo文字合成。缓存上限16项，每项普通/描边两个face，满时返回错误不删除仍被引用的face；缓存由进程退出析构释放。完整版文字布局、ruby/竖排和全游戏字形行为仍需后续执行层测试。

audio.h提供单路OGG BGM：复用现有libvorbisfile/libvorbis/libogg静态库，使用NSA范围流及32KiB PCM块，ALSA缓冲目标350ms，默认设备沿用系统default。不生成压缩或PCM缓存文件。生命周期为open、start、pause/volume/snapshot、stop、close，主线程管理start/stop，worker不依赖SDL。音量0~100%，暂停尽量用ALSA硬件pause，无此能力的设备暂停时可能耗尽已有PCM并在恢复后prepare；不能宣称所有蓝牙/声卡都已无缝验证。

尚未支持语音+BGM+音效多路混音、设备断连自动重开、游戏音频结束事件或脚本音频命令。因此单路音频自测通过不代表游戏全部音频已迁移。当前Vorbis为软件浮点，进一步降低CPU可评估整数解码器，但未替换现有库。

联合资源测试时显式设置ONS_NATIVE_AUDIO_ARCHIVE指向arc1.nsa，程序播放该测试游戏BGM并显示暂停按钮；默认不打开音频。此探针只做媒体并发验证，不会解释游戏脚本。

## 完整执行器目标

`make execution-objects`现在编译全部原ONScripter正文执行源（含文字、特效、存读档和原声音逻辑），不是重写一个少量命令解释器。runtime_image.cpp、runtime_display.cpp实现生成代码需要的原生图像、图片、字体、LVGL显示、触摸、事件和计时契约；接口采用原生名字和数据类型，没有SDL头或库。

`make ons-lvgl`是最终目标，仍因多路混音运行时未实现而链接失败，不能部署或称可玩。native_main.cpp会调用原openScript/init/executeLabel，完整启动的正确性尚未实测。PDA_AUTOSIZE保持640宽等比游戏显示，未改用户脚本。

runtime_display的计时器由独立线程调度；取消等待回调结束，回调不得调用LVGL。LVGL和像素提交只允许主线程。事件128项有界FIFO，可按类型peek/get筛选；motion可合并，按下/松开按顺序保留。触点先motion命中再button，释放到画面外取消旧按钮。timer当前32个固定slot，ID生命周期按原调用约定使用，不应持过期handle再取消重新分配的timer。

`native-runtime-image-tests`和`native-runtime-event-tests`验证这些接口，不等于完整游戏通过。显示脏区有90度旋转映射，完整执行器还需截图、触摸、音视频、多路音效、存读档与冷启动回归。

板端自测：

```sh
/root/aic_miracast/candidates/ons_native_gbkmap_20261004/native-tests \
  /root/roms/ons/saya/arc.nsa /root/roms/ons/saya/default.ttf
```

独占显示测试，返回菜单：

```sh
/etc/init.d/S90f1-menu stop
ONS_NATIVE_TEST_MS=30000 \
  /root/aic_miracast/candidates/ons_native_gbkmap_20261004/ons-native-probe-controls \
  /root/roms/ons/saya/arc.nsa /root/roms/ons/saya/default.ttf
/etc/init.d/S90f1-menu start
```

不设置ONS_NATIVE_TEST_MS则不定时退出；ONS_NATIVE_AUTO仅用于自动资源切换测试，默认不开启。箭头切换资源，X退出。游戏资源不进入Git/公开仓库。
