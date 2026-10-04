# ONS 原生后端移植设计

目前是可独立构建的媒体基础库和资源预览验证，不是完整ONS游戏引擎。目标是沿用已有CN/GBK脚本语义，不自行重写一个只认识少数命令的模拟器。旧ONS及SDL兼容候选保留，不修改系统SDL。

## 分层

| 功能 | 原依赖 | 目标/当前进度 |
|---|---|---|
| 脚本、标签、变量、存读档 | 现有ONS核心 | 待建立可复现源码基线并接入；保留既有语义 |
| NSA资源 | ONS读取接口 | 原生索引读取、大小写/斜杠匹配、LZSS已验证；SPB/NBZ/加密包待实现 |
| JPEG/PNG/BMP | SDL_image | libjpeg/libpng/原生BMP已验证；BMP目前BI_RGB的8/24/32位 |
| 像素缓冲、透明合成 | SDL_Surface/Blit | ARGB8888、直通alpha合成、矩形裁剪已验证；特效接入待做 |
| 中文字体 | SDL_ttf | FreeType+只读GBK码表已验证；字号可变、无需系统gconv |
| 显示、触摸 | SDL video/event | LVGL8.2 framebuffer与FT6236预览已显示；游戏事件适配待做 |
| 计时、线程 | SDL timer/thread | 后续POSIX monotonic/pthread，尚未接入 |
| OGG与混音 | SDL_mixer/SMPEG | 后续独立解码+ALSA有界PCM队列，尚未实现 |

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
